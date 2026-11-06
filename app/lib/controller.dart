import 'dart:async';
import 'dart:io';
import 'dart:convert';
import 'dart:math';

import 'package:flutter/foundation.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:http/http.dart' as http;
import 'package:path_provider/path_provider.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:wakelock_plus/wakelock_plus.dart';

import 'core.dart';

class ControlCentre extends ChangeNotifier {
  void changed() => notifyListeners();
  final FlutterSecureStorage secrets = const FlutterSecureStorage();
  late SharedPreferences prefs;
  late Directory directory;
  late Ledger ledger;
  Receiver? receiver;
  List<SdRow> sdRows = [];
  List<String> addresses = [];
  String apiKey = '',
      cloudKey = '',
      name = 'Parcel 1001',
      error = '',
      cloudMessage = 'Cloud relay off';
  String theme = 'system';
  int shipment = 1001, quietSeconds = 20, lostSeconds = 60, cloudCursor = 0;
  bool ready = false,
      demo = false,
      cloudEnabled = false,
      _mirroring = false,
      starting = false;
  Timer? _timer;
  final Map<String, String> stages = {};
  DateTime now = DateTime.now();
  bool get running => receiver?.running ?? false;
  List<CheckIn> get records => demo
      ? _samples
      : ledger.records.where((r) => r.shipment == shipment).toList();
  CheckIn? get latest => records.isEmpty ? null : records.last;
  Contact get contact =>
      contactState(latest?.received, now, quietSeconds, lostSeconds);
  String get endpoint => addresses.isEmpty
      ? 'Enable hotspot to find the phone IP'
      : 'http://${addresses.first}:8080';
  bool get demoThresholds => quietSeconds == 20 && lostSeconds == 60;
  late final List<CheckIn> _samples = List.generate(
    18,
    (i) => CheckIn(
      entry: i + 1,
      shipment: 1001,
      boot: 1,
      uptime: i * 20000,
      status: i == 8
          ? 2
          : i == 13
          ? 1
          : 0,
      events: i >= 13
          ? 2
          : i >= 8
          ? 1
          : 0,
      received: DateTime.now().subtract(Duration(seconds: (17 - i) * 20)),
      peak: i == 8
          ? 2.34
          : i == 13
          ? 0.76
          : 0.06 + (i % 5) * 0.03,
      lat: 1.309 - i * 0.00034,
      lon: 103.772 + i * 0.00065,
      source: 'sample',
    ),
  );

  Future<void> init() async {
    try {
      prefs = await SharedPreferences.getInstance();
      directory = await getApplicationSupportDirectory();
      apiKey = await secrets.read(key: 'receiver-key') ?? newApiKey();
      await secrets.write(key: 'receiver-key', value: apiKey);
      cloudKey = await secrets.read(key: 'cloud-key') ?? '';
      shipment = prefs.getInt('shipment') ?? 1001;
      name = prefs.getString('name') ?? 'Parcel $shipment';
      quietSeconds = prefs.getInt('quiet') ?? 20;
      lostSeconds = prefs.getInt('lost') ?? 60;
      theme = prefs.getString('theme') ?? 'system';
      cloudEnabled = prefs.getBool('cloud') ?? false;
      cloudCursor = prefs.getInt('cloudCursor') ?? 0;
      final saved = prefs.getString('stages');
      if (saved != null) {
        stages.addAll(Map<String, String>.from(jsonDecode(saved) as Map));
      }
      ledger = Ledger(File('${directory.path}/receiver.ndjson'));
      await ledger.open();
      final sd = File('${directory.path}/sd-import.csv');
      if (await sd.exists()) sdRows = parseSd(await sd.readAsString());
      await refreshAddresses();
      ready = true;
      _timer = Timer.periodic(const Duration(seconds: 1), (_) {
        now = DateTime.now();
        notifyListeners();
        if (now.second % 20 == 0) unawaited(mirrorNext());
      });
    } catch (e) {
      error = 'Could not open local records: $e';
    }
    notifyListeners();
  }

  Future<void> refreshAddresses() async {
    try {
      final interfaces = await NetworkInterface.list(
        type: InternetAddressType.IPv4,
      );
      addresses = [
        for (final i in interfaces)
          for (final a in i.addresses)
            if (!a.isLoopback && !a.address.startsWith('169.254.')) a.address,
      ];
      addresses.sort((a, b) => _rank(a).compareTo(_rank(b)));
    } catch (e) {
      error = 'Could not read network addresses: $e';
    }
    notifyListeners();
  }

  int _rank(String a) => a.startsWith('192.168.')
      ? 0
      : a.startsWith('10.')
      ? 1
      : 2;

  Future<void> toggleReceiver() async {
    if (starting) return;
    starting = true;
    notifyListeners();
    try {
      if (running) {
        await receiver!.stop();
        await WakelockPlus.disable();
      } else {
        demo = false;
        error = '';
        await refreshAddresses();
        receiver = Receiver(
          ledger: ledger,
          apiKey: apiKey,
          shipment: shipment,
          onRecord: (_) {
            now = DateTime.now();
            notifyListeners();
          },
          onError: (message) {
            error = message;
            notifyListeners();
          },
        );
        await receiver!.start();
        await WakelockPlus.enable();
      }
    } catch (e) {
      await receiver?.stop();
      error = 'Receiver could not start: $e';
    } finally {
      starting = false;
      notifyListeners();
    }
  }

  Future<void> configure({
    required String label,
    required int id,
    required int quiet,
    required int lost,
    required String key,
    required String appearance,
    required bool cloud,
    required String writeKey,
  }) async {
    if (quiet <= 0 || lost <= quiet) {
      throw const FormatException(
        'Lost timeout must be greater than quiet timeout.',
      );
    }
    if (id < 1 || id > 65535) {
      throw const FormatException('Shipment ID must be 1..65535.');
    }
    if (!RegExp(r'^[A-Za-z0-9_-]{16,32}$').hasMatch(key)) {
      throw const FormatException(
        'Receiver key must be 16..32 letters, digits, underscores or hyphens.',
      );
    }
    if (cloud && !RegExp(r'^[A-Za-z0-9]{16}$').hasMatch(writeKey)) {
      throw const FormatException(
        'Enter a valid 16-character ThingSpeak write key.',
      );
    }
    final wasRunning = running;
    if (wasRunning) await toggleReceiver();
    name = label.trim().isEmpty ? 'Parcel $id' : label.trim();
    shipment = id;
    quietSeconds = quiet;
    lostSeconds = lost;
    apiKey = key;
    theme = appearance;
    if (cloudKey != writeKey) {
      cloudCursor = 0;
      await prefs.setInt('cloudCursor', 0);
    }
    cloudEnabled = cloud;
    cloudKey = writeKey;
    await secrets.write(key: 'receiver-key', value: apiKey);
    await secrets.write(key: 'cloud-key', value: cloudKey);
    await prefs.setString('name', name);
    await prefs.setInt('shipment', shipment);
    await prefs.setInt('quiet', quiet);
    await prefs.setInt('lost', lost);
    await prefs.setString('theme', theme);
    await prefs.setBool('cloud', cloudEnabled);
    if (wasRunning) await toggleReceiver();
    notifyListeners();
  }

  String firmwareConfig(String host) =>
      '''
// Copy into firmware/ParcelGuardian/Secrets.h (gitignored).
// Set WIFI_SSID and WIFI_PASSWORD to your 2.4 GHz WPA2 phone hotspot.
#define WIFI_SSID "YOUR_HOTSPOT_NAME"
#define WIFI_PASSWORD "YOUR_HOTSPOT_PASSWORD"
#define PHONE_SERVER_HOST "$host"
#define PHONE_API_KEY "$apiKey"
#define SHIPMENT_ID $shipment
''';

  Future<void> importSd(String text) async {
    final parsed = parseSd(
      text,
    ); // Validate before replacing the previous import.
    if (parsed.isEmpty) {
      throw const FormatException('The file contains no SD records.');
    }
    final temp = File('${directory.path}/sd-import.tmp');
    await temp.writeAsString(text, flush: true);
    await temp.rename('${directory.path}/sd-import.csv');
    sdRows = parsed;
    notifyListeners();
  }

  Future<void> setStage(CheckIn record, String value) async {
    stages[record.identity] = value;
    await prefs.setString('stages', jsonEncode(stages));
    notifyListeners();
  }

  String get csv {
    final lines = [
      'entry,received_utc,shipment,session,uptime_ms,status,peak_delta_g,total_events,latitude,longitude,source,leg',
    ];
    String quote(String s) => '"${s.replaceAll('"', '""')}"';
    for (final r in records) {
      lines.add(
        [
          r.entry,
          r.received.toUtc().toIso8601String(),
          r.shipment,
          r.boot,
          r.uptime,
          r.status,
          r.peak,
          r.events,
          r.lat ?? '',
          r.lon ?? '',
          r.source,
          quote(stages[r.identity] ?? ''),
        ].join(','),
      );
    }
    return '${lines.join('\n')}\n';
  }

  Future<void> mirrorNext() async {
    if (!ready || !cloudEnabled || cloudKey.isEmpty || _mirroring) return;
    final pending = ledger.records.where((r) => r.entry > cloudCursor).toList();
    if (pending.isEmpty) {
      cloudMessage = 'Cloud relay caught up';
      return;
    }
    _mirroring = true;
    try {
      final r = pending.first;
      final reply = await http
          .post(
            Uri.https('api.thingspeak.com', '/update'),
            body: {
              'api_key': cloudKey,
              'field1': r.lat?.toString() ?? '',
              'field2': r.lon?.toString() ?? '',
              'field3': r.peak.toString(),
              'field4': '${r.status}',
              'field5': '${r.events}',
              'status':
                  'Parcel ${r.shipment} session ${r.boot}; phone received ${r.received.toUtc().toIso8601String()}',
            },
          )
          .timeout(const Duration(seconds: 8));
      if (reply.statusCode != 200 ||
          (int.tryParse(reply.body.trim()) ?? 0) <= 0) {
        throw const HttpException('ThingSpeak rejected the write');
      }
      cloudCursor = r.entry;
      await prefs.setInt('cloudCursor', cloudCursor);
      cloudMessage =
          'Mirrored #${r.entry} · ${max(0, pending.length - 1)} pending';
    } catch (_) {
      cloudMessage = 'Cloud unavailable · local records retained';
    } finally {
      _mirroring = false;
      notifyListeners();
    }
  }

  @override
  void dispose() {
    _timer?.cancel();
    unawaited(receiver?.stop() ?? Future.value());
    unawaited(WakelockPlus.disable());
    super.dispose();
  }
}
