// MPDJ (2026-11-16): workspace recovery. At launch MPDJ copies the card's Workspace
// (project, patterns, programs) to Workspace.restore, unless the last session didn't
// end cleanly: then the copy from before that session is kept. File > Restore marks it
// to be copied back at the next launch (before the workspace opens), so a session that
// went wrong can be undone. MIT, see NOTICE.md (MPDJ additions).
#pragma once

namespace Recovery
{
// before the tracker starts: restore if asked, else take the launch snapshot
void atLaunch();
// on a clean exit (the snapshot may be replaced next launch)
void atCleanExit();
}
