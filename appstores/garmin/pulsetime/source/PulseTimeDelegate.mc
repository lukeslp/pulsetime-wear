// File purpose: Input for the full app view. Select/tap replays; Up/Down or
// swipe cycles the time system; Back exits; Menu is a no-op.
import Toybox.Lang;
import Toybox.WatchUi;

class PulseTimeDelegate extends WatchUi.BehaviorDelegate {
    private var _view as PulseTimeView;

    function initialize(view as PulseTimeView) {
        BehaviorDelegate.initialize();
        _view = view;
    }

    // Tap on the touch screen or the Start/Enter key.
    function onSelect() as Boolean {
        _view.playNow();
        return true;
    }

    // Down key / swipe up.
    function onNextPage() as Boolean {
        _view.cycleSystem(1);
        return true;
    }

    // Up key / swipe down.
    function onPreviousPage() as Boolean {
        _view.cycleSystem(-1);
        return true;
    }

    // Let the system pop the app; onHide cancels playback.
    function onBack() as Boolean {
        return false;
    }

    function onMenu() as Boolean {
        return true;
    }
}
