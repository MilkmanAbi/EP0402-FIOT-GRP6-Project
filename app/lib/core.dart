import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';

enum Contact { waiting, live, quiet, lost }

Contact contactState(DateTime? last, DateTime now, int quiet, int lost) {
  if (last == null) return Contact.waiting;
  final seconds = max(0, now.difference(last).inSeconds);
  if (seconds >= lost) return Contact.lost;
  if (seconds >= quiet) return Contact.quiet;
  return Contact.live;
}

String newApiKey() {
  final random = Random.secure();
  return List.generate(24, (_) => random.nextInt(16).toRadixString(16)).join();
}

class CheckIn {
  final int entry, shipment, boot, uptime, status, events;
  final DateTime received;
  final double peak;
  final double? lat, lon;
  final String source;
  const CheckIn({
    required this.entry,
    required this.shipment,
    required this.boot,
    required this.uptime,
    required this.status,
    required this.events,
    required this.received,
    required this.peak,
    this.lat,
    this.lon,
    this.source = 'receiver',
  });
  bool get hasFix => lat != null && lon != null;
  String get identity => '$shipment:$boot:$uptime';
  String get severity => switch (status) {
    1 => 'Rough handling',
    2 => 'Severe impact',
    3 => 'No GPS fix',
    _ => 'Normal',
  };
  Map<String, dynamic> toJson() => {
    'entry': entry,
    'shipment': shipment,
    'boot': boot,
    'uptime': uptime,
    'status': status,
    'events': events,
    'received': received.toUtc().toIso8601String(),
    'peak': peak,
    'lat': lat,
    'lon': lon,
    'source': source,
  };
  factory CheckIn.fromJson(Map<String, dynamic> v) => CheckIn(
    entry: v['entry'] as int,
    shipment: v['shipment'] as int,
    boot: v['boot'] as int,
    uptime: v['uptime'] as int,
    status: v['status'] as int,
    events: v['events'] as int,
    received: DateTime.parse(v['received'] as String),
    peak: (v['peak'] as num).toDouble(),
    lat: (v['lat'] as num?)?.toDouble(),
    lon: (v['lon'] as num?)?.toDouble(),
    source: v['source'] as String? ?? 'receiver',
  );
  factory CheckIn.fromQuery(Map<String, String> q, int entry, DateTime now) {
    int integer(String key, int min, int max) {
      final s = q[key] ?? '';
      if (!RegExp(r'^\d+$').hasMatch(s)) throw FormatException('Invalid $key');
      final n = int.tryParse(s);
      if (n == null || n < min || n > max) {
        throw FormatException('Invalid $key');
      }
      return n;
    }

    double number(String key, double min, double max) {
      final n = double.tryParse(q[key] ?? '');
      if (n == null || !n.isFinite || n < min || n > max) {
        throw FormatException('Invalid $key');
      }
      return n;
    }

    final status = integer('field4', 0, 3);
    final a = q['field1'] ?? '', b = q['field2'] ?? '';
    if (a.isEmpty != b.isEmpty) {
      throw const FormatException('Incomplete GPS fix');
    }
    if (status == 3 && a.isNotEmpty) {
      throw const FormatException('No-fix row has coordinates');
    }
    return CheckIn(
      entry: entry,
      shipment: integer('id', 1, 65535),
      boot: integer('boot', 0, 65535),
      uptime: integer('ms', 0, 4294967295),
      status: status,
      events: integer('field5', 0, 4294967295),
      received: now.toUtc(),
      peak: number('field3', 0, 40),
      lat: a.isEmpty ? null : number('field1', -90, 90),
      lon: b.isEmpty ? null : number('field2', -180, 180),
    );
  }
}

// A durable append-only receiver journal, separate from the SD black box.
// An acknowledgement is returned only after the journal write is flushed.
class Ledger {
  final File file;
  final List<CheckIn> records = [];
  final Map<String, CheckIn> _seen = {};
  int _next = 1;
  Future<void> _tail = Future.value();
  Ledger(this.file);
  Future<void> open() async {
    await file.parent.create(recursive: true);
    if (!await file.exists()) {
      await file.create();
      return;
    }
    final bytes = await file.readAsBytes();
    // Discard only a torn final append, so the next row starts cleanly.
    final end = bytes.lastIndexOf(10) + 1;
    if (end < bytes.length) {
      final handle = await file.open(mode: FileMode.append);
      await handle.truncate(end);
      await handle.close();
    }
    for (final line in utf8.decode(bytes.sublist(0, end)).split('\n')) {
      if (line.isEmpty) continue;
      // Corruption in a completed row is surfaced, never silently erased.
      final record = CheckIn.fromJson(jsonDecode(line) as Map<String, dynamic>);
      records.add(record);
      _seen[record.identity] = record;
      _next = max(_next, record.entry + 1);
    }
  }

  Future<CheckIn> accept(Map<String, String> query, int expectedShipment) {
    final done = Completer<CheckIn>();
    _tail = _tail.then((_) async {
      try {
        final candidate = CheckIn.fromQuery(query, _next, DateTime.now());
        if (candidate.shipment != expectedShipment) {
          throw const FormatException('Wrong shipment ID');
        }
        final previous = _seen[candidate.identity];
        if (previous != null) {
          // A retry must carry the same payload, not quietly overwrite evidence.
          if (previous.status != candidate.status ||
              previous.events != candidate.events ||
              previous.peak != candidate.peak ||
              previous.lat != candidate.lat ||
              previous.lon != candidate.lon) {
            throw const FormatException('Conflicting duplicate');
          }
          done.complete(previous);
          return;
        }
        await file.writeAsString(
          '${jsonEncode(candidate.toJson())}\n',
          mode: FileMode.append,
          flush: true,
        );
        records.add(candidate);
        _seen[candidate.identity] = candidate;
        _next++;
        done.complete(candidate);
      } catch (e, st) {
        done.completeError(e, st);
      }
    });
    return done.future;
  }
}

class Receiver {
  final Ledger ledger;
  final String apiKey;
  final int shipment;
  final void Function(CheckIn) onRecord;
  final void Function(String) onError;
  HttpServer? _server;
  bool get running => _server != null;
  int get port => _server?.port ?? 8080;
  Receiver({
    required this.ledger,
    required this.apiKey,
    required this.shipment,
    required this.onRecord,
    required this.onError,
  });
  Future<void> start({int port = 8080, InternetAddress? address}) async {
    if (_server != null) return;
    _server = await HttpServer.bind(
      address ?? InternetAddress.anyIPv4,
      port,
      shared: false,
    );
    _server!.idleTimeout = const Duration(seconds: 5);
    _server!.listen((request) {
      unawaited(_handle(request));
    }, onError: (Object e) => onError('$e'));
  }

  Future<void> stop() async {
    await _server?.close(force: true);
    _server = null;
  }

  Future<void> _handle(HttpRequest request) async {
    final response = request.response;
    int code = 200;
    String body = '0\n';
    try {
      if (request.method != 'GET') {
        code = 405;
      } else if (request.uri.toString().length > 1024) {
        code = 414;
      } else if (request.uri.path == '/health') {
        body = 'ParcelGuardian receiver\n';
      } else if (request.uri.path != '/update') {
        code = 404;
      } else if (request.uri.queryParametersAll.values.any(
        (v) => v.length != 1,
      )) {
        code = 400;
      } else if (!_sameKey(
        request.uri.queryParameters['api_key'] ?? '',
        apiKey,
      )) {
        code = 401;
      } else {
        final record = await ledger.accept(
          request.uri.queryParameters,
          shipment,
        );
        body = '${record.entry}\n';
        onRecord(record);
      }
    } on FormatException {
      code = 400;
    } catch (e) {
      code = 503;
      onError('Could not save check-in: $e');
    }
    response.statusCode = code;
    response.headers.contentType = ContentType.text;
    response.headers.set(HttpHeaders.cacheControlHeader, 'no-store');
    response.persistentConnection = false;
    final bytes = utf8.encode(body);
    response.contentLength = bytes.length;
    try {
      response.add(bytes);
      await response.close();
    } catch (_) {
      /* client disconnected */
    }
  }

  static bool _sameKey(String a, String b) {
    if (a.length != b.length) return false;
    var diff = 0;
    for (var i = 0; i < a.length; i++) {
      diff |= a.codeUnitAt(i) ^ b.codeUnitAt(i);
    }
    return diff == 0;
  }
}

class SdRow {
  final String utc, state, event, session;
  final int ms, events;
  final double delta, peak;
  final double? lat, lon;
  final bool uploaded;
  SdRow(
    this.utc,
    this.state,
    this.event,
    this.session,
    this.ms,
    this.events,
    this.delta,
    this.peak,
    this.lat,
    this.lon,
    this.uploaded,
  );
}

List<SdRow> parseSd(String text) {
  final rows = <SdRow>[];
  var session = 'Unknown session';
  var lineNumber = 0;
  for (final line in const LineSplitter().convert(text)) {
    lineNumber++;
    final s = line.trim();
    if (s.isEmpty || s.startsWith('ms,') || s.startsWith('----')) continue;
    if (s.startsWith('#')) {
      session = s.substring(1).trim();
      continue;
    }
    final c = s.split(',');
    if (c.length != 12) {
      throw FormatException('SD CSV line $lineNumber: expected 12 columns');
    }
    double finite(String value) {
      final n = double.tryParse(value);
      if (n == null || !n.isFinite) {
        throw FormatException('Invalid number on line $lineNumber');
      }
      return n;
    }

    final fix = c[10] == '1';
    if (c[10] != '0' && !fix) {
      throw FormatException('Invalid fix on line $lineNumber');
    }
    final lat = fix ? finite(c[7]) : null, lon = fix ? finite(c[8]) : null;
    if (fix && (lat!.abs() > 90 || lon!.abs() > 180)) {
      throw FormatException('GPS range on line $lineNumber');
    }
    rows.add(
      SdRow(
        c[1],
        c[2],
        c[3],
        session,
        int.parse(c[0]),
        int.parse(c[6]),
        finite(c[4]),
        finite(c[5]),
        lat,
        lon,
        c[11] == '1',
      ),
    );
  }
  return rows;
}
