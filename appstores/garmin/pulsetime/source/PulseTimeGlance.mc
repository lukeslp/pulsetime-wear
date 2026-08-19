// File purpose: Glance-loop preview: the app name and the current local time.
// Tapping the glance opens the full widget, which vibrates the time at once.
import Toybox.Graphics;
import Toybox.Lang;
import Toybox.System;
import Toybox.WatchUi;

(:glance)
class PulseTimeGlance extends WatchUi.GlanceView {
    function initialize() {
        GlanceView.initialize();
    }

    function onUpdate(dc as Dc) as Void {
        dc.setColor(Graphics.COLOR_WHITE, Graphics.COLOR_TRANSPARENT);
        dc.clear();
        var h = dc.getHeight();
        dc.drawText(0, h / 4, Graphics.FONT_GLANCE, "PulseTime",
                    Graphics.TEXT_JUSTIFY_LEFT | Graphics.TEXT_JUSTIFY_VCENTER);
        dc.drawText(0, (3 * h) / 4, Graphics.FONT_GLANCE_NUMBER, PulseTimeGlance.timeText(),
                    Graphics.TEXT_JUSTIFY_LEFT | Graphics.TEXT_JUSTIFY_VCENTER);
    }

    // Local time as "h:mm" or "HH:mm" following the watch's 12/24-hour setting.
    static function timeText() as String {
        var now = System.getClockTime();
        var hour = now.hour;
        var settings = System.getDeviceSettings();
        if (!settings.is24Hour) {
            hour = hour % 12;
            if (hour == 0) { hour = 12; }
        }
        return hour.format("%d") + ":" + now.min.format("%02d");
    }
}
