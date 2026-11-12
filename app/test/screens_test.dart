import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:parcel_guardian/controller.dart';
import 'package:parcel_guardian/core.dart';
import 'package:parcel_guardian/main.dart';

class _TestControlCentre extends ControlCentre {
  @override
  Future<void> refreshAddresses() async {
    addresses = ['10.0.0.8', '192.168.43.1'];
    changed();
  }
}

void main() {
  testWidgets(
    'Empty dashboard and settings fit a narrow phone with enlarged text',
    (tester) async {
      tester.view.physicalSize = const Size(360, 800);
      tester.view.devicePixelRatio = 1;
      addTearDown(tester.view.resetPhysicalSize);
      addTearDown(tester.view.resetDevicePixelRatio);
      final m = ControlCentre();
      m.ledger = Ledger(File('unused-test-journal'));
      await tester.pumpWidget(
        MaterialApp(
          theme: ThemeData(useMaterial3: true),
          home: MediaQuery(
            data: const MediaQueryData(
              size: Size(360, 800),
              textScaler: TextScaler.linear(1.25),
            ),
            child: Home(model: m),
          ),
        ),
      );
      expect(find.text('AWAITING TRACKER'), findsOneWidget);
      expect(tester.takeException(), isNull);
      await tester.tap(find.text('Settings'));
      await tester.pumpAndSettle();
      expect(find.text('Pair your tracker'), findsOneWidget);
      expect(tester.takeException(), isNull);
      await tester.tap(find.text('Evidence'));
      await tester.pumpAndSettle();
      expect(find.text('No check-ins in this view'), findsOneWidget);
      expect(tester.takeException(), isNull);
    },
  );

  testWidgets('Address refresh keeps the selected hotspot', (tester) async {
    final model = _TestControlCentre()..hotspotHost = '192.168.43.1';
    addTearDown(model.dispose);
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: ListenableBuilder(
            listenable: model,
            builder: (_, _) => SettingsPanel(model: model),
          ),
        ),
      ),
    );
    final field = find.widgetWithText(TextField, 'Hotspot IPv4 address');
    String address() => tester.widget<TextField>(field).controller!.text;
    expect(address(), '192.168.43.1');
    await tester.tap(find.text('Refresh phone addresses'));
    await tester.pumpAndSettle();
    expect(find.text('10.0.0.8'), findsOneWidget);
    expect(address(), '192.168.43.1');
    expect(model.endpoint, 'http://192.168.43.1:8080');
    expect(
      model.firmwareConfig(),
      contains('PHONE_SERVER_HOST "192.168.43.1"'),
    );
  });

  test('Default contact stays live through the normal upload interval', () {
    final model = ControlCentre();
    addTearDown(model.dispose);
    final last = DateTime.utc(2026);
    Contact at(int seconds) => contactState(
      last,
      last.add(Duration(seconds: seconds)),
      model.quietSeconds,
      model.lostSeconds,
    );
    expect(at(20), Contact.live);
    expect(at(30), Contact.quiet);
    expect(at(60), Contact.lost);
  });
}
