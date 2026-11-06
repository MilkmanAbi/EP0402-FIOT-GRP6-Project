import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:parcel_guardian/core.dart';

Map<String, String> query({
  String ms = '20000',
  String lat = '1.3',
  String lon = '103.8',
  String status = '0',
}) => {
  'api_key': 'test_key_12345678',
  'id': '1001',
  'boot': '1',
  'ms': ms,
  'field1': lat,
  'field2': lon,
  'field3': '0.12',
  'field4': status,
  'field5': '2',
};

void main() {
  test('Contact clock uses receipt, with exact quiet/lost boundaries', () {
    final now = DateTime.utc(2026, 11, 6);
    expect(contactState(null, now, 20, 60), Contact.waiting);
    for (final pair in [
      (19, Contact.live),
      (20, Contact.quiet),
      (59, Contact.quiet),
      (60, Contact.lost),
    ]) {
      expect(
        contactState(now.subtract(Duration(seconds: pair.$1)), now, 20, 60),
        pair.$2,
      );
    }
    expect(
      contactState(now.add(const Duration(seconds: 5)), now, 20, 60),
      Contact.live,
    );
  });
  test('No fix remains valid contact and severe impact survives no fix', () {
    final r = CheckIn.fromQuery(
      query(lat: '', lon: '', status: '2'),
      1,
      DateTime.now(),
    );
    expect(r.hasFix, false);
    expect(r.status, 2);
    expect(
      CheckIn.fromQuery(
        query(lat: '', lon: '', status: '3'),
        1,
        DateTime.now(),
      ).hasFix,
      false,
    );
    expect(
      CheckIn.fromQuery(query(lat: '0', lon: '0'), 1, DateTime.now()).hasFix,
      true,
    );
  });
  test('Reject malformed and implausible telemetry', () {
    for (final change in [
      {'field1': 'NaN'},
      {'field1': 'Infinity'},
      {'field1': '91'},
      {'field2': '-181'},
      {'field1': ''},
      {'field4': '3'},
      {'field4': '4'},
      {'field5': '-1'},
      {'id': '65536'},
      {'boot': '65536'},
      {'ms': '4294967296'},
      {'field3': 'NaN'},
      {'field3': '41'},
    ]) {
      expect(
        () => CheckIn.fromQuery({...query(), ...change}, 1, DateTime.now()),
        throwsFormatException,
      );
    }
  });
  group('Durable journal and live HTTP receiver', () {
    late Directory temp;
    late Ledger ledger;
    late Receiver receiver;
    setUp(() async {
      temp = await Directory.systemTemp.createTemp('pg-test-');
      ledger = Ledger(File('${temp.path}/ledger.ndjson'));
      await ledger.open();
      receiver = Receiver(
        ledger: ledger,
        apiKey: 'test_key_12345678',
        shipment: 1001,
        onRecord: (_) {},
        onError: (_) {},
      );
      await receiver.start(port: 0, address: InternetAddress.loopbackIPv4);
    });
    tearDown(() async {
      await receiver.stop();
      await temp.delete(recursive: true);
    });
    Future<(int, String)> send(
      Map<String, String> q, {
      String method = 'GET',
      String path = '/update',
    }) async {
      final client = HttpClient();
      try {
        final req = await client.openUrl(
          method,
          Uri.http('127.0.0.1:${receiver.port}', path, q),
        );
        final res = await req.close();
        return (res.statusCode, await utf8.decoder.bind(res).join());
      } finally {
        client.close(force: true);
      }
    }

    test('Acknowledges a flushed record and reloads after restart', () async {
      expect(await send(query()), (200, '1\n'));
      final restored = Ledger(ledger.file);
      await restored.open();
      expect(restored.records.single.lat, 1.3);
      expect(restored.records.single.events, 2);
    });
    test(
      'Authentication and schema errors never update last contact',
      () async {
        expect((await send({...query(), 'api_key': 'wrong'})).$1, 401);
        expect((await send({...query(), 'id': '2002'})).$1, 400);
        expect((await send({...query(), 'field1': 'NaN'})).$1, 400);
        expect((await send(query(), method: 'POST')).$1, 405);
        expect((await send(query(), path: '/records')).$1, 404);
        expect(ledger.records, isEmpty);
      },
    );
    test(
      'Idempotent retries do not refresh receipt time or create rows',
      () async {
        expect(await send(query()), (200, '1\n'));
        final first = ledger.records.single.received;
        expect(await send(query()), (200, '1\n'));
        expect(ledger.records.length, 1);
        expect(ledger.records.single.received, first);
        expect((await send({...query(), 'field3': '1.2'})).$1, 400);
      },
    );
    test('Concurrent requests have unique monotonic entry IDs', () async {
      final replies = await Future.wait(
        List.generate(12, (i) => send(query(ms: '${20000 + i}'))),
      );
      expect(replies.every((r) => r.$1 == 200), true);
      expect(ledger.records.map((r) => r.entry).toSet().length, 12);
      final restored = Ledger(ledger.file);
      await restored.open();
      expect(restored.records.length, 12);
    });
    test('A torn final append is removed before a new append', () async {
      await send(query());
      await ledger.file.writeAsString(
        '{"entry":',
        mode: FileMode.append,
        flush: true,
      );
      final restored = Ledger(ledger.file);
      await restored.open();
      await restored.accept(query(ms: '40000'), 1001);
      final twice = Ledger(ledger.file);
      await twice.open();
      expect(twice.records.length, 2);
      expect(twice.records.last.entry, 2);
    });
    test(
      'A failed storage write is never acknowledged or retained in memory',
      () async {
        await ledger.file.delete();
        await Directory(ledger.file.path).create();
        expect((await send(query())).$1, 503);
        expect(ledger.records, isEmpty);
      },
    );
  });
  test('SD CSV preserves sessions, no-fix and offline impacts', () {
    const csv =
        'ms,utc,state,event,delta_g,peak_g,events,lat,lon,sats,fix,uploaded\n'
        '# session 1 shipment 1001 firmware ParcelGuardian\n'
        '2000,no-time,TRANSIT,SEVERE,1.800,1.800,1,,,0,0,0\n'
        '# session 2 shipment 1001 firmware ParcelGuardian\n'
        '10000,2026-11-06 09:00:00,TRANSIT,HB,0.100,0.100,1,0,0,8,1,1\n';
    final rows = parseSd(csv);
    expect(rows.length, 2);
    expect(rows.first.lat, isNull);
    expect(rows.first.uploaded, false);
    expect(rows.first.session, contains('session 1'));
    expect(rows.last.lat, 0);
    expect(rows.last.session, contains('session 2'));
    expect(() => parseSd('1,broken'), throwsFormatException);
  });
}
