"""HUD unmount must cancel toast-queue CallLater callbacks.

CloseMikesUI previously left ProcessNotificationQueue / HideCurrentAndProcessNext
scheduled. After DisplayStopDraw those fire on a torn-down HUD and NPE in the
legacy SetText path (AO complete + contributor toasts, then death/respawn).

Run: python -m unittest discover -s tools -p "test_notification_queue_unmount.py"
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "Scripts/Game"


def source(path):
    return (GAME / path).read_text(encoding="utf-8-sig")


def method(text, name):
    match = re.search(
        r"^\s*(?:(?:static|override|protected|private)\s+)*\w+\s+"
        + re.escape(name)
        + r"\([^;{}]*\)\s*\{",
        text,
        re.MULTILINE,
    )
    if not match:
        raise AssertionError(f"Missing method: {name}")
    start = match.end()
    depth = 1
    for i in range(start, len(text)):
        depth += (text[i] == "{") - (text[i] == "}")
        if depth == 0:
            return text[start:i]
    raise AssertionError(f"Unclosed method: {name}")


class NotificationQueueUnmountTests(unittest.TestCase):
    def test_close_mikes_ui_removes_queue_callbacks(self):
        display = source("IA_NotificationDisplay.c")
        close = method(display, "CloseMikesUI")
        self.assertIn("Remove(this.ProcessNotificationQueue)", close)
        self.assertIn("Remove(this.HideCurrentAndProcessNext)", close)
        self.assertIn("m_bIsDisplaying = false", close)
        self.assertIn("m_notificationQueue.Clear()", close)
        self.assertLess(
            close.index("Remove(this.ProcessNotificationQueue)"),
            close.index("m_HudHost.Close()"),
        )

    def test_process_queue_refuses_unmounted_hud(self):
        display = source("IA_NotificationDisplay.c")
        process = method(display, "ProcessNotificationQueue")
        self.assertIn("CanPresentNotification()", process)
        can = method(display, "CanPresentNotification")
        self.assertIn("m_Runtime && m_Toast", can)
        self.assertIn("m_RedText || m_wInfoText || m_YellowText", can)

    def test_toast_finished_does_not_reschedule_after_unmount(self):
        display = source("IA_NotificationDisplay.c")
        finished = method(display, "OnToastFinished")
        self.assertIn("if (!m_Runtime || !m_Toast)", finished)
        self.assertLess(
            finished.index("if (!m_Runtime || !m_Toast)"),
            finished.index("CallLater(this.ProcessNotificationQueue"),
        )

    def test_legacy_show_null_checks_widgets(self):
        display = source("IA_NotificationDisplay.c")
        show = method(display, "_InternalShowNotification")
        self.assertIn("if (!m_RedText && !m_wInfoText && !m_YellowText)", show)
        self.assertIn("if (m_RedText)", show)
        self.assertIn("if (m_wInfoText)", show)
        self.assertIn("if (m_YellowText)", show)


if __name__ == "__main__":
    unittest.main()
