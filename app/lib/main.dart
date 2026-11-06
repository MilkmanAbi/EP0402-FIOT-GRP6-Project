import 'dart:io';
import 'dart:convert';
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart' show LatLng;
import 'package:intl/intl.dart';
import 'package:file_picker/file_picker.dart';
import 'package:share_plus/share_plus.dart';
import 'package:url_launcher/url_launcher.dart';

import 'controller.dart';
import 'core.dart';

const ink = Color(0xFF122C4A),
    teal = Color(0xFF087F78),
    amber = Color(0xFFB86A13),
    red = Color(0xFFCE4653);
void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const GuardianApp());
}

class GuardianApp extends StatefulWidget {
  const GuardianApp({super.key});
  @override
  State<GuardianApp> createState() => _GuardianAppState();
}

class _GuardianAppState extends State<GuardianApp> {
  final model = ControlCentre();
  @override
  void initState() {
    super.initState();
    model.init();
  }

  @override
  void dispose() {
    model.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => ListenableBuilder(
    listenable: model,
    builder: (context, _) {
      ThemeData theme(Brightness b) => ThemeData(
        useMaterial3: true,
        brightness: b,
        colorScheme: ColorScheme.fromSeed(seedColor: teal, brightness: b),
        scaffoldBackgroundColor: b == Brightness.light
            ? const Color(0xFFF3F6F8)
            : const Color(0xFF101B27),
        appBarTheme: const AppBarTheme(
          centerTitle: false,
          scrolledUnderElevation: 0,
        ),
        cardTheme: CardThemeData(
          elevation: 0,
          margin: EdgeInsets.zero,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(20),
          ),
        ),
        inputDecorationTheme: InputDecorationTheme(
          border: OutlineInputBorder(borderRadius: BorderRadius.circular(12)),
        ),
      );
      return MaterialApp(
        title: 'ParcelGuardian',
        debugShowCheckedModeBanner: false,
        theme: theme(Brightness.light),
        darkTheme: theme(Brightness.dark),
        themeMode: switch (model.theme) {
          'light' => ThemeMode.light,
          'dark' => ThemeMode.dark,
          _ => ThemeMode.system,
        },
        home: model.ready
            ? Home(model: model)
            : Scaffold(
                body: Center(
                  child: Padding(
                    padding: const EdgeInsets.all(24),
                    child: model.error.isEmpty
                        ? const CircularProgressIndicator()
                        : Text(model.error),
                  ),
                ),
              ),
      );
    },
  );
}

String timeLabel(DateTime time) =>
    DateFormat('d MMM, HH:mm:ss').format(time.toLocal());
String ageLabel(DateTime? time, DateTime now) {
  if (time == null) return 'No check-in yet';
  final s = math.max(0, now.difference(time).inSeconds);
  if (s < 60) return '${s}s ago';
  if (s < 3600) return '${s ~/ 60}m ago';
  if (s < 86400) return '${s ~/ 3600}h ago';
  return '${s ~/ 86400}d ago';
}

Color contactColor(Contact c) => switch (c) {
  Contact.live => teal,
  Contact.quiet => amber,
  Contact.lost => red,
  _ => Colors.blueGrey,
};
Color severityColor(int s) => s == 2
    ? red
    : s == 1
    ? amber
    : teal;

class Home extends StatefulWidget {
  final ControlCentre model;
  const Home({super.key, required this.model});
  @override
  State<Home> createState() => _HomeState();
}

class _HomeState extends State<Home> {
  int tab = 0;
  ControlCentre get m => widget.model;
  void message(String text) =>
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(text)));
  Future<void> export() async {
    try {
      final file = File(
        '${m.directory.path}/${m.demo ? 'sample' : 'parcel'}-${m.shipment}.csv',
      );
      await file.writeAsString(m.csv, flush: true);
      await SharePlus.instance.share(
        ShareParams(
          files: [XFile(file.path)],
          text:
              'ParcelGuardian ${m.demo ? 'sample data' : 'receiver check-ins'}',
        ),
      );
    } catch (e) {
      if (mounted) message('Export failed: $e');
    }
  }

  Future<void> import() async {
    try {
      final picked = await FilePicker.pickFile(
        type: FileType.custom,
        allowedExtensions: ['csv', 'txt'],
      );
      if (picked == null) return;
      final text = utf8.decode(await picked.readAsBytes());
      await m.importSd(text);
      if (mounted) {
        message(
          'Imported ${m.sdRows.length} SD rows. Live check-in time is unchanged.',
        );
      }
    } catch (e) {
      if (mounted) {
        message('Import failed: $e');
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final titles = [
      'Control centre',
      'Journey map',
      'Evidence log',
      'Journey insights',
      'Settings',
    ];
    final pages = [
      Dashboard(
        model: m,
        onMap: () => setState(() => tab = 1),
        onSettings: () => setState(() => tab = 4),
      ),
      JourneyMap(model: m),
      Evidence(model: m, onImport: import, onExport: export),
      Insights(model: m),
      SettingsPanel(model: m),
    ];
    return Scaffold(
      appBar: AppBar(
        title: Row(
          children: [
            Container(
              padding: const EdgeInsets.all(8),
              decoration: BoxDecoration(
                color: teal,
                borderRadius: BorderRadius.circular(12),
              ),
              child: const Icon(
                Icons.shield_outlined,
                color: Colors.white,
                size: 22,
              ),
            ),
            const SizedBox(width: 10),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'ParcelGuardian',
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: Theme.of(context).textTheme.titleMedium
                        ?.copyWith(fontWeight: FontWeight.w800),
                  ),
                  Text(
                    titles[tab],
                    style: Theme.of(context).textTheme.labelSmall,
                  ),
                ],
              ),
            ),
          ],
        ),
        actions: [
          IconButton(
            tooltip: 'Setup guide',
            onPressed: () => showGuide(context),
            icon: const Icon(Icons.help_outline),
          ),
        ],
      ),
      body: Column(
        children: [
          if (m.demo)
            Container(
              width: double.infinity,
              color: amber.withValues(alpha: 0.15),
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
              child: Row(
                children: [
                  const Icon(Icons.science_outlined, size: 18, color: amber),
                  const SizedBox(width: 8),
                  const Expanded(
                    child: Text('SAMPLE DATA · isolated from real records'),
                  ),
                  TextButton(
                    onPressed: () {
                      m.demo = false;
                      m.changed();
                    },
                    child: const Text('Exit'),
                  ),
                ],
              ),
            ),
          if (m.error.isNotEmpty)
            MaterialBanner(
              content: Text(m.error),
              actions: [
                TextButton(
                  onPressed: () {
                    m.error = '';
                    m.changed();
                  },
                  child: const Text('Dismiss'),
                ),
              ],
            ),
          Expanded(child: pages[tab]),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: tab,
        onDestinationSelected: (i) => setState(() => tab = i),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.space_dashboard_outlined),
            selectedIcon: Icon(Icons.space_dashboard),
            label: 'Overview',
          ),
          NavigationDestination(
            icon: Icon(Icons.map_outlined),
            selectedIcon: Icon(Icons.map),
            label: 'Map',
          ),
          NavigationDestination(
            icon: Icon(Icons.receipt_long_outlined),
            label: 'Evidence',
          ),
          NavigationDestination(
            icon: Icon(Icons.bar_chart_outlined),
            label: 'Insights',
          ),
          NavigationDestination(icon: Icon(Icons.tune), label: 'Settings'),
        ],
      ),
    );
  }
}

class Section extends StatelessWidget {
  final String title;
  final Widget child;
  final String? subtitle;
  const Section(this.title, this.child, {super.key, this.subtitle});
  @override
  Widget build(BuildContext context) => Card(
    child: Padding(
      padding: const EdgeInsets.all(20),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            title,
            style: Theme.of(context).textTheme.titleMedium
                ?.copyWith(fontWeight: FontWeight.w700),
          ),
          if (subtitle != null) ...[
            const SizedBox(height: 4),
            Text(subtitle!, style: Theme.of(context).textTheme.bodySmall),
          ],
          const SizedBox(height: 16),
          child,
        ],
      ),
    ),
  );
}

class Metric extends StatelessWidget {
  final String value, label;
  final IconData icon;
  const Metric(this.value, this.label, this.icon, {super.key});
  @override
  Widget build(BuildContext context) => Expanded(
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Icon(icon, size: 21, color: teal),
        const SizedBox(height: 8),
        Text(
          value,
          style: Theme.of(context).textTheme.headlineSmall
              ?.copyWith(fontWeight: FontWeight.w800),
        ),
        Text(label, style: Theme.of(context).textTheme.bodySmall),
      ],
    ),
  );
}

class Dashboard extends StatelessWidget {
  final ControlCentre model;
  final VoidCallback onMap, onSettings;
  const Dashboard({
    super.key,
    required this.model,
    required this.onMap,
    required this.onSettings,
  });
  @override
  Widget build(BuildContext context) {
    final m = model,
        r = m.latest,
        c = m.contact,
        color = contactColor(c),
        fixes = m.records.where((r) => r.hasFix).toList();
    final fix = fixes.isEmpty ? null : fixes.last;
    return ListView(
      padding: const EdgeInsets.all(20),
      children: [
        Text(
          'Every journey leaves a record.',
          style: Theme.of(context).textTheme.headlineMedium
              ?.copyWith(fontWeight: FontWeight.w800, letterSpacing: -0.8),
        ),
        const SizedBox(height: 6),
        Text(m.name, style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 20),
        Container(
          padding: const EdgeInsets.all(24),
          decoration: BoxDecoration(
            color: ink,
            borderRadius: BorderRadius.circular(24),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Container(
                    width: 9,
                    height: 9,
                    decoration: BoxDecoration(
                      color: color,
                      shape: BoxShape.circle,
                    ),
                  ),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      c == Contact.waiting
                          ? 'AWAITING TRACKER'
                          : c.name.toUpperCase(),
                      style: TextStyle(
                        color: color == teal ? const Color(0xFF68DED1) : color,
                        fontWeight: FontWeight.w800,
                        letterSpacing: 1.5,
                      ),
                    ),
                  ),
                  const Icon(Icons.inventory_2_outlined, color: Colors.white70),
                ],
              ),
              const SizedBox(height: 18),
              Text(
                ageLabel(r?.received, m.now),
                style: const TextStyle(
                  color: Colors.white,
                  fontSize: 36,
                  fontWeight: FontWeight.w800,
                ),
              ),
              const SizedBox(height: 8),
              Text(switch (c) {
                Contact.waiting => 'Start the receiver and connect your tracker to this phone’s hotspot.',
                Contact.live => 'The phone received a check-in recently.',
                Contact.quiet => 'The tracker has missed the quiet window. It may be out of Wi-Fi range.',
                Contact.lost => 'No contact beyond your lost threshold. This does not establish theft or damage.',
              }, style: const TextStyle(color: Colors.white70, height: 1.5)),
              if (r != null) ...[
                const SizedBox(height: 16),
                Text(
                  'RECEIVED  ${timeLabel(r.received)}',
                  style: const TextStyle(color: Colors.white60, fontSize: 11),
                ),
              ],
            ],
          ),
        ),
        const SizedBox(height: 16),
        Section(
          'Journey at a glance',
          Row(
            children: [
              Metric('${r?.events ?? 0}', 'Cumulative events', Icons.sensors),
              Metric(
                m.records.isEmpty
                    ? '—'
                    : '${m.records.map((r) => r.peak).reduce(math.max).toStringAsFixed(2)} g',
                'Peak reported Δg',
                Icons.show_chart,
              ),
              Metric('${m.records.length}', 'Check-ins', Icons.wifi_tethering),
            ],
          ),
        ),
        const SizedBox(height: 16),
        Section(
          'Phone receiver',
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Icon(
                    m.running ? Icons.wifi_tethering : Icons.wifi_tethering_off,
                    color: m.running ? teal : Colors.blueGrey,
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: Text(
                      m.running ? 'Listening on port 8080' : 'Receiver stopped',
                      style: const TextStyle(fontWeight: FontWeight.w700),
                    ),
                  ),
                  FilledButton(
                    onPressed: m.starting ? null : m.toggleReceiver,
                    child: Text(m.running ? 'Stop' : 'Start'),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Text(m.endpoint),
              const SizedBox(height: 8),
              const Text(
                'Keep this app open during the receiver session. The screen stays awake. Enable your 2.4 GHz hotspot in Android settings.',
              ),
              TextButton(
                onPressed: onSettings,
                child: const Text('Pair tracker & configure timeouts →'),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        Section(
          'Last reported position',
          Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                fix == null
                    ? 'No valid GPS position received'
                    : '${fix.lat!.toStringAsFixed(6)}, ${fix.lon!.toStringAsFixed(6)}',
                style: Theme.of(context).textTheme.titleMedium,
              ),
              const SizedBox(height: 8),
              Text(
                fix == null
                    ? 'GPS may need outdoor sky view. No position has been invented.'
                    : '${timeLabel(fix.received)} · ${r?.hasFix == true ? 'GPS fix in latest check-in' : 'Historical fix; latest check-in has no GPS fix'}',
              ),
              const SizedBox(height: 12),
              OutlinedButton.icon(
                onPressed: onMap,
                icon: const Icon(Icons.map_outlined),
                label: const Text('Explore journey map'),
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        const Section(
          'Evidence stays with the parcel',
          Text(
            'The microSD card holds the full black-box record. Phone check-ins contain only what reached this receiver. Import PGLOG.CSV at handover to inspect the complete recorded journey.',
          ),
        ),
        const SizedBox(height: 16),
        Text(
          'Quiet ${m.quietSeconds}s · Lost ${m.lostSeconds}s${m.demoThresholds ? ' · demo timing' : ''}',
        ),
        const SizedBox(height: 24),
      ],
    );
  }
}

class JourneyMap extends StatefulWidget {
  final ControlCentre model;
  const JourneyMap({super.key, required this.model});
  @override
  State<JourneyMap> createState() => _JourneyMapState();
}

class _JourneyMapState extends State<JourneyMap> {
  final map = MapController();
  bool sd = false;
  @override
  void dispose() {
    map.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final m = widget.model;
    final points = sd
        ? m.sdRows
              .where((r) => r.lat != null)
              .map((r) => LatLng(r.lat!, r.lon!))
              .toList()
        : m.records
              .where((r) => r.hasFix)
              .map((r) => LatLng(r.lat!, r.lon!))
              .toList();
    final last = points.isEmpty ? null : points.last;
    final events = sd
        ? m.sdRows
              .where(
                (r) =>
                    r.lat != null &&
                    (r.event == 'ROUGH' || r.event == 'SEVERE'),
              )
              .map((r) => (LatLng(r.lat!, r.lon!), r.event == 'SEVERE' ? 2 : 1))
              .toList()
        : m.records
              .where((r) => r.hasFix && (r.status == 1 || r.status == 2))
              .map((r) => (LatLng(r.lat!, r.lon!), r.status))
              .toList();
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(20, 8, 20, 12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              SegmentedButton<bool>(
                segments: const [
                  ButtonSegment(value: false, label: Text('Check-ins')),
                  ButtonSegment(value: true, label: Text('SD black box')),
                ],
                selected: {sd},
                onSelectionChanged: (v) => setState(() => sd = v.first),
              ),
              const SizedBox(height: 10),
              Text(
                points.isEmpty
                    ? 'No GPS fixes in this source.'
                    : '${points.length} recorded fixes · ${events.length} impact points',
              ),
              const Text(
                'Lines connect samples, not the proven route between them.',
                style: TextStyle(fontSize: 12),
              ),
            ],
          ),
        ),
        Expanded(
          child: Stack(
            children: [
              FlutterMap(
                mapController: map,
                options: MapOptions(
                  initialCenter: last ?? const LatLng(1.3521, 103.8198),
                  initialZoom: 14,
                  minZoom: 2,
                  maxZoom: 19,
                ),
                children: [
                  TileLayer(
                    urlTemplate:
                        'https://tile.openstreetmap.org/{z}/{x}/{y}.png',
                    userAgentPackageName: 'sg.edu.sp.grp6.parcel_guardian',
                  ),
                  if (points.length > 1)
                    PolylineLayer(
                      polylines: [
                        Polyline(points: points, strokeWidth: 4, color: teal),
                      ],
                    ),
                  MarkerLayer(
                    markers: [
                      for (final e in events)
                        Marker(
                          point: e.$1,
                          width: 34,
                          height: 34,
                          child: Tooltip(
                            message: e.$2 == 2
                                ? 'Severe impact'
                                : 'Rough handling',
                            child: Icon(
                              Icons.warning_rounded,
                              color: severityColor(e.$2),
                              size: 30,
                            ),
                          ),
                        ),
                      if (last != null)
                        Marker(
                          point: last,
                          width: 44,
                          height: 44,
                          child: const Icon(
                            Icons.location_on,
                            color: ink,
                            size: 42,
                          ),
                        ),
                    ],
                  ),
                ],
              ),
              Positioned(
                left: 8,
                bottom: 8,
                child: Material(
                  color: Colors.white.withValues(alpha: 0.95),
                  borderRadius: BorderRadius.circular(6),
                  child: InkWell(
                    onTap: () => launchUrl(
                      Uri.parse('https://www.openstreetmap.org/copyright'),
                    ),
                    child: const Padding(
                      padding: EdgeInsets.all(6),
                      child: Text(
                        '© OpenStreetMap contributors',
                        style: TextStyle(color: ink, fontSize: 11),
                      ),
                    ),
                  ),
                ),
              ),
              if (points.isEmpty)
                Positioned(
                  top: 16,
                  left: 20,
                  right: 20,
                  child: Card(
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Text(
                        sd ? 'Import PGLOG.CSV in Evidence to see SD fixes.' : 'Waiting for a tracker fix. Singapore is shown as an overview.',
                      ),
                    ),
                  ),
                ),
              Positioned(
                right: 16,
                bottom: 44,
                child: Column(
                  children: [
                    FloatingActionButton.small(
                      heroTag: 'centre',
                      tooltip: 'Centre last fix',
                      onPressed: last == null ? null : () => map.move(last, 15),
                      child: const Icon(Icons.my_location),
                    ),
                    const SizedBox(height: 8),
                    FloatingActionButton.small(
                      heroTag: 'maps',
                      tooltip: 'Open last fix in Google Maps',
                      onPressed: last == null
                          ? null
                          : () => launchUrl(
                              Uri.https('www.google.com', '/maps/search/', {
                                'api': '1',
                                'query': '${last.latitude},${last.longitude}',
                              }),
                              mode: LaunchMode.externalApplication,
                            ),
                      child: const Icon(Icons.open_in_new),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
        const Padding(
          padding: EdgeInsets.all(12),
          child: Text(
            'Basemap needs internet. Stored coordinates remain available offline.',
            style: TextStyle(fontSize: 12),
          ),
        ),
      ],
    );
  }
}

class Evidence extends StatefulWidget {
  final ControlCentre model;
  final VoidCallback onImport, onExport;
  const Evidence({
    super.key,
    required this.model,
    required this.onImport,
    required this.onExport,
  });
  @override
  State<Evidence> createState() => _EvidenceState();
}

class _EvidenceState extends State<Evidence> {
  bool sd = false;
  int filter = -1;
  @override
  Widget build(BuildContext context) {
    final m = widget.model,
        records = m.records.reversed
            .where((r) => filter < 0 || r.status == filter)
            .toList();
    final rows = m.sdRows.reversed
        .where(
          (r) => filter < 0 || r.event == (filter == 2 ? 'SEVERE' : 'ROUGH'),
        )
        .toList();
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Expanded(
                    child: SegmentedButton<bool>(
                      segments: const [
                        ButtonSegment(value: false, label: Text('Received')),
                        ButtonSegment(value: true, label: Text('SD log')),
                      ],
                      selected: {sd},
                      onSelectionChanged: (v) => setState(() => sd = v.first),
                    ),
                  ),
                  IconButton(
                    tooltip: 'Import PGLOG.CSV',
                    onPressed: widget.onImport,
                    icon: const Icon(Icons.file_open_outlined),
                  ),
                  IconButton(
                    tooltip: 'Share receiver CSV',
                    onPressed: widget.onExport,
                    icon: const Icon(Icons.ios_share),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Wrap(
                spacing: 8,
                children: [
                  for (final v in [-1, 1, 2])
                    ChoiceChip(
                      label: Text(
                        v < 0
                            ? 'All'
                            : v == 1
                            ? 'Rough'
                            : 'Severe',
                      ),
                      selected: filter == v,
                      onSelected: (_) => setState(() => filter = v),
                    ),
                ],
              ),
              const SizedBox(height: 8),
              Text(
                sd
                    ? 'Imported SD data does not reset the contact clock.'
                    : 'Tap a check-in to label its journey leg.',
                style: Theme.of(context).textTheme.bodySmall,
              ),
              const SizedBox(height: 12),
            ],
          ),
        ),
        Expanded(
          child: (sd ? rows.isEmpty : records.isEmpty)
              ? Center(
                  child: Padding(
                    padding: const EdgeInsets.all(32),
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        const Icon(
                          Icons.receipt_long_outlined,
                          size: 48,
                          color: Colors.blueGrey,
                        ),
                        const SizedBox(height: 16),
                        Text(
                          sd
                              ? 'No SD records in this view'
                              : 'No check-ins in this view',
                        ),
                        const SizedBox(height: 8),
                        Text(
                          sd
                              ? 'Import PGLOG.CSV from the card or a saved serial dump.'
                              : 'Start the receiver on Overview, or explore sample mode in Settings.',
                          textAlign: TextAlign.center,
                        ),
                      ],
                    ),
                  ),
                )
              : ListView.builder(
                  padding: const EdgeInsets.fromLTRB(20, 0, 20, 20),
                  itemCount: sd ? rows.length : records.length,
                  itemBuilder: (context, i) {
                    if (sd) {
                      final r = rows[i],
                          impact = r.event == 'ROUGH' || r.event == 'SEVERE';
                      return Padding(
                        padding: const EdgeInsets.only(bottom: 8),
                        child: Card(
                          child: ListTile(
                            leading: Icon(
                              impact
                                  ? Icons.warning_amber_rounded
                                  : Icons.fiber_manual_record_outlined,
                              color: r.event == 'SEVERE'
                                  ? red
                                  : impact
                                  ? amber
                                  : teal,
                            ),
                            title: Text(
                              '${r.event} · ${r.delta.toStringAsFixed(3)} Δg',
                            ),
                            subtitle: Text(
                              '${r.utc} UTC\n${r.session}\n${r.state} · ${r.ms} ms · ${r.lat == null ? 'No GPS fix' : '${r.lat}, ${r.lon}'}',
                            ),
                            isThreeLine: true,
                          ),
                        ),
                      );
                    }
                    final r = records[i];
                    return Padding(
                      padding: const EdgeInsets.only(bottom: 8),
                      child: Card(
                        child: ListTile(
                          onTap: () => detail(context, r),
                          leading: CircleAvatar(
                            backgroundColor: severityColor(r.status)
                                .withValues(alpha: 0.12),
                            child: Icon(
                              r.status == 2
                                  ? Icons.warning_rounded
                                  : r.status == 1
                                  ? Icons.sensors
                                  : Icons.check,
                              color: severityColor(r.status),
                            ),
                          ),
                          title: Text(
                            r.severity,
                            style: const TextStyle(fontWeight: FontWeight.w700),
                          ),
                          subtitle: Text(
                            '${timeLabel(r.received)}\n${m.stages[r.identity] ?? 'Unlabelled leg'} · ${r.hasFix ? 'GPS fix' : 'No GPS fix'}',
                          ),
                          trailing: Text(
                            '${r.peak.toStringAsFixed(2)} g',
                            style: const TextStyle(fontWeight: FontWeight.w800),
                          ),
                          isThreeLine: true,
                        ),
                      ),
                    );
                  },
                ),
        ),
      ],
    );
  }

  Future<void> detail(BuildContext context, CheckIn r) async {
    final stage = TextEditingController(
      text: widget.model.stages[r.identity] ?? '',
    );
    await showModalBottomSheet<void>(
      context: context,
      isScrollControlled: true,
      useSafeArea: true,
      builder: (context) => Padding(
        padding: EdgeInsets.fromLTRB(
          24,
          24,
          24,
          24 + MediaQuery.viewInsetsOf(context).bottom,
        ),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Check-in #${r.entry}',
              style: Theme.of(context).textTheme.headlineSmall,
            ),
            const SizedBox(height: 12),
            Text(
              '${r.severity} · peak ${r.peak.toStringAsFixed(3)} Δg\nReceived ${r.received.toUtc().toIso8601String()}\nShipment ${r.shipment} · session ${r.boot}\nDevice uptime ${r.uptime} ms · cumulative events ${r.events}\n${r.hasFix ? 'GPS: ${r.lat}, ${r.lon}' : 'GPS: no fix'}',
            ),
            const SizedBox(height: 20),
            TextField(
              controller: stage,
              maxLength: 80,
              decoration: const InputDecoration(
                labelText: 'Journey leg',
                hintText: 'Origin depot, van, handover…',
              ),
            ),
            const SizedBox(height: 12),
            FilledButton(
              onPressed: () async {
                await widget.model.setStage(r, stage.text.trim());
                if (context.mounted) Navigator.pop(context);
              },
              child: const Text('Save label'),
            ),
          ],
        ),
      ),
    );
    stage.dispose();
  }
}

class Insights extends StatefulWidget {
  final ControlCentre model;
  const Insights({super.key, required this.model});
  @override
  State<Insights> createState() => _InsightsState();
}

class _InsightsState extends State<Insights> {
  bool sd = false;
  @override
  Widget build(BuildContext context) {
    final m = widget.model, records = m.records;
    final values = sd
        ? m.sdRows
              .where((r) => r.event == 'ROUGH' || r.event == 'SEVERE')
              .map((r) => r.delta)
              .toList()
        : records.map((r) => r.peak).toList();
    final rough = sd
        ? m.sdRows.where((r) => r.event == 'ROUGH').length
        : records.where((r) => r.status == 1).length;
    final severe = sd
        ? m.sdRows.where((r) => r.event == 'SEVERE').length
        : records.where((r) => r.status == 2).length;
    final total = sd ? m.sdRows.length : records.length,
        fixes = sd
            ? m.sdRows.where((r) => r.lat != null).length
            : records.where((r) => r.hasFix).length;
    var gap = 0;
    for (var i = 1; i < records.length; i++) {
      gap = math.max(
        gap,
        records[i].received.difference(records[i - 1].received).inSeconds,
      );
    }
    final legs = <String, int>{};
    for (final r in records.where((r) => r.status == 1 || r.status == 2)) {
      final key = m.stages[r.identity] ?? 'Unlabelled';
      legs[key] = (legs[key] ?? 0) + 1;
    }
    Widget bar(String label, int count, Color color) => Padding(
      padding: const EdgeInsets.only(bottom: 14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(child: Text(label)),
              Text(
                '$count',
                style: const TextStyle(fontWeight: FontWeight.w800),
              ),
            ],
          ),
          const SizedBox(height: 6),
          LinearProgressIndicator(
            value: total == 0 ? 0 : count / total,
            minHeight: 8,
            color: color,
            borderRadius: BorderRadius.circular(6),
          ),
        ],
      ),
    );
    return ListView(
      padding: const EdgeInsets.all(20),
      children: [
        Text(
          'Understand the journey.',
          style: Theme.of(context).textTheme.headlineMedium
              ?.copyWith(fontWeight: FontWeight.w800),
        ),
        const SizedBox(height: 16),
        SegmentedButton<bool>(
          segments: const [
            ButtonSegment(value: false, label: Text('Check-ins')),
            ButtonSegment(value: true, label: Text('SD black box')),
          ],
          selected: {sd},
          onSelectionChanged: (v) => setState(() => sd = v.first),
        ),
        const SizedBox(height: 16),
        Section(
          'Recorded observations',
          Row(
            children: [
              Metric(
                '$total',
                sd ? 'SD rows' : 'Check-ins',
                Icons.receipt_long,
              ),
              Metric(
                total == 0 ? '—' : '${(fixes / total * 100).round()}%',
                'GPS fix rate',
                Icons.satellite_alt,
              ),
              Metric(
                values.isEmpty
                    ? '—'
                    : '${values.reduce(math.max).toStringAsFixed(2)} g',
                'Peak Δg',
                Icons.show_chart,
              ),
            ],
          ),
        ),
        const SizedBox(height: 16),
        Section(
          sd ? 'Impact events' : 'Impact-bearing check-ins',
          Column(
            children: [bar('Rough', rough, amber), bar('Severe', severe, red)],
          ),
          subtitle: sd ? 'Counted from ROUGH and SEVERE rows.' : 'One check-in can summarise multiple events. These bars are not exact event counts.',
        ),
        const SizedBox(height: 16),
        Section(
          sd ? 'Impact magnitude' : 'Peak per check-in',
          SizedBox(
            height: 150,
            width: double.infinity,
            child: values.isEmpty
                ? const Center(child: Text('Waiting for recorded data'))
                : CustomPaint(painter: Sparkline(values, teal)),
          ),
          subtitle: 'Δg relative to the resting baseline. This does not establish damage.',
        ),
        const SizedBox(height: 16),
        if (!sd) ...[
          Section(
            'Connectivity gaps',
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  records.length < 2 ? 'Two check-ins needed' : '${gap}s',
                  style: Theme.of(context).textTheme.headlineMedium
                      ?.copyWith(fontWeight: FontWeight.w800),
                ),
                const Text(
                  'Longest observed interval between received check-ins. Summaries cannot measure exact Wi-Fi coverage.',
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          Section(
            'Reported impacts by journey leg',
            legs.isEmpty
                ? const Text(
                    'Label impact check-ins in Evidence to compare journey legs.',
                  )
                : Column(
                    children: [
                      for (final e in legs.entries) bar(e.key, e.value, teal),
                    ],
                  ),
          ),
          const SizedBox(height: 16),
        ],
        const Text(
          'SD and phone records are separate sources. Phone history can miss offline events. Calibration results must come from physical tests.',
        ),
      ],
    );
  }
}

class Sparkline extends CustomPainter {
  final List<double> values;
  final Color color;
  Sparkline(this.values, this.color);
  @override
  void paint(Canvas canvas, Size size) {
    final max = math.max(1.2, values.reduce(math.max)),
        paint = Paint()
          ..color = color
          ..strokeWidth = 2.5
          ..style = PaintingStyle.stroke;
    for (final t in [0.4, 1.2]) {
      final y = size.height - t / max * (size.height - 16);
      canvas.drawLine(
        Offset(0, y),
        Offset(size.width, y),
        Paint()
          ..color = amber.withValues(alpha: 0.35)
          ..strokeWidth = 1,
      );
    }
    final path = Path();
    for (var i = 0; i < values.length; i++) {
      final x = values.length == 1
              ? size.width / 2
              : i / (values.length - 1) * size.width,
          y = size.height - values[i] / max * (size.height - 16);
      if (i == 0) {
        path.moveTo(x, y);
      } else {
        path.lineTo(x, y);
      }
      canvas.drawCircle(Offset(x, y), 3, Paint()..color = color);
    }
    canvas.drawPath(path, paint);
  }

  @override
  bool shouldRepaint(Sparkline old) =>
      old.values != values || old.color != color;
}

class SettingsPanel extends StatefulWidget {
  final ControlCentre model;
  const SettingsPanel({super.key, required this.model});
  @override
  State<SettingsPanel> createState() => _SettingsPanelState();
}

class _SettingsPanelState extends State<SettingsPanel> {
  late final TextEditingController label, id, quiet, lost, key, cloudKey, host;
  late bool cloud;
  late String appearance;
  bool obscure = true, saving = false;
  ControlCentre get m => widget.model;
  @override
  void initState() {
    super.initState();
    label = TextEditingController(text: m.name);
    id = TextEditingController(text: '${m.shipment}');
    quiet = TextEditingController(text: '${m.quietSeconds}');
    lost = TextEditingController(text: '${m.lostSeconds}');
    key = TextEditingController(text: m.apiKey);
    cloudKey = TextEditingController(text: m.cloudKey);
    host = TextEditingController(
      text: m.addresses.isEmpty ? '' : m.addresses.first,
    );
    cloud = m.cloudEnabled;
    appearance = m.theme;
  }

  @override
  void dispose() {
    for (final c in [label, id, quiet, lost, key, cloudKey, host]) {
      c.dispose();
    }
    super.dispose();
  }

  void message(String value) =>
      ScaffoldMessenger.of(context)
          .showSnackBar(SnackBar(content: Text(value)));
  Future<void> save() async {
    setState(() => saving = true);
    try {
      await m.configure(
        label: label.text,
        id: int.parse(id.text),
        quiet: int.parse(quiet.text),
        lost: int.parse(lost.text),
        key: key.text.trim(),
        appearance: appearance,
        cloud: cloud,
        writeKey: cloudKey.text.trim(),
      );
      if (mounted) {
        message('Settings saved. Copy the config if pairing changed.');
      }
    } catch (e) {
      if (mounted) {
        message('Settings not saved: $e');
      }
    } finally {
      if (mounted) {
        setState(() => saving = false);
      }
    }
  }

  @override
  Widget build(BuildContext context) => ListView(
    padding: const EdgeInsets.all(20),
    children: [
      Section(
        'Pair your tracker',
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              '1. Enable a 2.4 GHz WPA2 hotspot on this phone.\n2. Refresh addresses and choose the hotspot IPv4.\n3. Save settings, copy the config, compile and upload the Uno.\n4. Start the receiver on Overview.',
            ),
            const SizedBox(height: 16),
            OutlinedButton.icon(
              onPressed: () async {
                await m.refreshAddresses();
                if (mounted) {
                  setState(() {
                    if (m.addresses.isNotEmpty) {
                      host.text = m.addresses.first;
                    }
                  });
                }
              },
              icon: const Icon(Icons.refresh),
              label: const Text('Refresh phone addresses'),
            ),
            for (final address in m.addresses)
              ListTile(
                dense: true,
                title: Text(address),
                leading: const Icon(Icons.router_outlined),
                onTap: () => setState(() => host.text = address),
              ),
            const SizedBox(height: 12),
            TextField(
              controller: host,
              decoration: const InputDecoration(
                labelText: 'Hotspot IPv4 address',
                helperText: 'Use the address reachable from a hotspot client.',
              ),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: key,
              obscureText: obscure,
              maxLength: 32,
              decoration: InputDecoration(
                labelText: 'Receiver API key',
                suffixIcon: IconButton(
                  tooltip: 'Show or hide key',
                  onPressed: () => setState(() => obscure = !obscure),
                  icon: Icon(
                    obscure
                        ? Icons.visibility_outlined
                        : Icons.visibility_off_outlined,
                  ),
                ),
              ),
            ),
            Wrap(
              spacing: 8,
              children: [
                TextButton.icon(
                  onPressed: () => setState(() => key.text = newApiKey()),
                  icon: const Icon(Icons.key),
                  label: const Text('Generate new key'),
                ),
                OutlinedButton.icon(
                  onPressed: () async {
                    if (key.text.trim() != m.apiKey ||
                        int.tryParse(id.text) != m.shipment) {
                      message('Save pairing settings before copying.');
                      return;
                    }
                    if (InternetAddress.tryParse(host.text.trim())?.type !=
                        InternetAddressType.IPv4) {
                      message('Enter the hotspot IPv4 address.');
                      return;
                    }
                    await Clipboard.setData(
                      ClipboardData(text: m.firmwareConfig(host.text.trim())),
                    );
                    if (mounted) {
                      message('Firmware config copied. Keep it private.');
                    }
                  },
                  icon: const Icon(Icons.copy),
                  label: const Text('Copy firmware config'),
                ),
              ],
            ),
          ],
        ),
      ),
      const SizedBox(height: 16),
      Section(
        'Shipment & contact windows',
        Column(
          children: [
            TextField(
              controller: label,
              maxLength: 80,
              decoration: const InputDecoration(labelText: 'Parcel name'),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: id,
              keyboardType: TextInputType.number,
              decoration: const InputDecoration(
                labelText: 'Shipment ID (1–65535)',
              ),
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: quiet,
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: 'Quiet after (seconds)',
                    ),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: TextField(
                    controller: lost,
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: 'Lost after (seconds)',
                    ),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              children: [
                TextButton(
                  onPressed: () => setState(() {
                    quiet.text = '20';
                    lost.text = '60';
                  }),
                  child: const Text('Demo: 20s / 60s'),
                ),
                TextButton(
                  onPressed: () => setState(() {
                    quiet.text = '86400';
                    lost.text = '2592000';
                  }),
                  child: const Text('Journey: 24h / 30d'),
                ),
              ],
            ),
            const Text(
              'LOST means no contact beyond the threshold. It is an operator alert, not proof of loss.',
            ),
          ],
        ),
      ),
      const SizedBox(height: 16),
      Section(
        'Optional cloud relay',
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Mirror to ThingSpeak over HTTPS'),
              value: cloud,
              onChanged: (v) => setState(() => cloud = v),
            ),
            const Text(
              'Local reception works without internet. One queued check-in is relayed every 20 seconds while the app is open.',
            ),
            const SizedBox(height: 16),
            TextField(
              controller: cloudKey,
              obscureText: true,
              decoration: const InputDecoration(
                labelText: 'ThingSpeak write API key',
              ),
            ),
            const SizedBox(height: 12),
            Text(m.cloudEnabled ? m.cloudMessage : 'Cloud relay off'),
          ],
        ),
      ),
      const SizedBox(height: 16),
      Section(
        'Appearance',
        DropdownButtonFormField<String>(
          initialValue: appearance,
          items: const [
            DropdownMenuItem(value: 'system', child: Text('Follow system')),
            DropdownMenuItem(value: 'light', child: Text('Light')),
            DropdownMenuItem(value: 'dark', child: Text('Dark')),
          ],
          onChanged: (v) => setState(() => appearance = v!),
        ),
      ),
      const SizedBox(height: 16),
      FilledButton.icon(
        onPressed: saving ? null : save,
        icon: const Icon(Icons.save_outlined),
        label: Text(saving ? 'Saving…' : 'Save settings'),
      ),
      const SizedBox(height: 24),
      Section(
        'Explore the app',
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Sample mode shows an illustrative route and impacts. It never writes to the receiver journal or the cloud.',
            ),
            const SizedBox(height: 12),
            OutlinedButton.icon(
              onPressed: m.running
                  ? null
                  : () {
                      m.demo = !m.demo;
                      m.changed();
                    },
              icon: const Icon(Icons.science_outlined),
              label: Text(
                m.demo ? 'Exit sample mode' : 'Explore sample journey',
              ),
            ),
          ],
        ),
      ),
      const SizedBox(height: 16),
      const Text(
        'ParcelGuardian · EP0402 Group 6\nUno R3 + tracker shield\nLocal HTTP on a private hotspot · SD is the system of record.\nKeep the app open during reception. No phone GPS collection or account required.',
        style: TextStyle(fontSize: 12, height: 1.6),
      ),
      const SizedBox(height: 24),
    ],
  );
}

void showGuide(BuildContext context) => showModalBottomSheet<void>(
  context: context,
  isScrollControlled: true,
  useSafeArea: true,
  builder: (context) => const SingleChildScrollView(
    padding: EdgeInsets.all(24),
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          'Your phone is the control centre.',
          style: TextStyle(fontSize: 25, fontWeight: FontWeight.w800),
        ),
        SizedBox(height: 16),
        Text(
          'The Uno detects impacts, sounds the local alert and logs to microSD. The ESP-01 joins this phone’s hotspot and sends summaries to the app on port 8080.\n\n'
          'Pairing\nEnable a 2.4 GHz WPA2 hotspot in Android settings. In the app, refresh addresses, save the pairing key and copy the firmware configuration. Put it in Secrets.h with your hotspot credentials. Compile and upload, then start the receiver.\n\n'
          'During the demo\nKeep this app open. The screen stays awake during a receiver session. Walk outside for GPS sky view. A no-fix check-in is still a valid check-in. Switch the hotspot off to demonstrate LIVE → QUIET → LOST; restore it to recover.\n\n'
          'At handover\nUse READOUT DIP mode, or send d over serial at 9600 baud. Save PGLOG.CSV and import it in Evidence. The SD record includes offline events.\n\n'
          'Limits of the evidence\nAn impact is not proof of damage. LOST means missed contact, not theft. Map lines connect samples and cannot establish the route between them. Physical calibration is still required.\n\n'
          'Network troubleshooting\nSome phones isolate hotspot clients or change IP after restarting. Test /health from a connected laptop. The app does not enable or configure the hotspot automatically.\n\n'
          'Maps and privacy\nOpenStreetMap supplies the basemap online. Google Maps opens when tapped. Receiver records stay in app storage unless exported or cloud relay is enabled. Uninstalling removes local records; export first.\n\n'
          'Demo transport\nLocal HTTP authenticates a sender with a pairing key but does not encrypt traffic or sign evidence. Use a private hotspot and demonstration data.',
        ),
      ],
    ),
  ),
);
