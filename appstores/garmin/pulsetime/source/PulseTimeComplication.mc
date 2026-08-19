// File purpose: publishes PulseTime's one public complication, so a watch face
// can host it and launch the app (which vibrates the time) with a tap.
//
// The value is the current local time in the watch's 12/24-hour format. There
// is deliberately no Background service behind it: Connect IQ cannot vibrate
// from a background process, so a temporal event would only churn the battery
// without ever being able to do PulseTime's one job. The complication is a
// launcher with a readable face, and it refreshes whenever the app runs.
import Toybox.Complications;
import Toybox.Lang;

module PulseTimeComplication {
    // Matches <complication id="0"> in resources/complications/complications.xml;
    // updateComplication takes that id as its index.
    const INDEX = 0;

    // Push the current time into the published complication. Guarded twice so
    // the app still runs on firmware without the Complications module (it
    // arrived in Connect IQ 4.2.0); on those devices this is a no-op.
    function publish() as Void {
        if (!(Toybox has :Complications) || !(Complications has :updateComplication)) {
            return;
        }
        Complications.updateComplication(INDEX, {:value => PulseTimeGlance.timeText()});
    }
}
