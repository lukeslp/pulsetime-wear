// File purpose: Connect IQ entry point for PulseTime (device app with a
// glance and one published complication).
import Toybox.Application;
import Toybox.Lang;
import Toybox.WatchUi;

(:glance)
class PulseTimeApp extends Application.AppBase {
    private var _view as PulseTimeView?;

    function initialize() {
        AppBase.initialize();
    }

    // Deliberately does not publish the complication. AppBase.onStart also
    // runs in the glance code space, which carries only (:glance) symbols, so
    // a call to PulseTimeComplication from here would fail in the glance.
    // PulseTimeView.playNow publishes instead - on every open and every replay.
    function onStart(state as Dictionary?) as Void {
    }

    // The AppBase runs in both the glance and full-app scopes, so the members
    // that touch the full view are excluded from the glance type check.
    (:typecheck(disableGlanceCheck))
    function onStop(state as Dictionary?) as Void {
        if (_view != null) {
            _view.stopPlayback();
        }
    }

    (:typecheck(disableGlanceCheck))
    function getInitialView() as [Views] or [Views, InputDelegates] {
        var view = new PulseTimeView();
        _view = view;
        return [view, new PulseTimeDelegate(view)];
    }

    (:glance)
    function getGlanceView() as [GlanceView] or [GlanceView, GlanceViewDelegate] or Null {
        return [new PulseTimeGlance()];
    }

    (:typecheck(disableGlanceCheck))
    function onSettingsChanged() as Void {
        if (_view != null) {
            _view.reloadSettings();
        }
        WatchUi.requestUpdate();
    }
}
