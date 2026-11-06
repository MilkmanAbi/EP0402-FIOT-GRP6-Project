import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:parcel_guardian/controller.dart';
import 'package:parcel_guardian/core.dart';
import 'package:parcel_guardian/main.dart';

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
}
