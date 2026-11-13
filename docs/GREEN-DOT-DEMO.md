# Green dot: the actual ESP display

The task: **“Open heysalad.ai and give me three improvements.”**

On 13 November 2026, Codex used the Windows browser through the computer-use driver while OpenPromptlet displayed its live checkpoints. The ESP32-S3 drew the moving dot, checkpoint ring, current step and elapsed time. The host sent normalized state updates after the corresponding browser observations. The screen acknowledged counts 0, 1, 2, 3, 4 and 5. Completion followed the written findings, rather than a timer.

The phone recording is `PXL_20261113_214341443.TS.mp4`, 160.064444 seconds. Published highlights keep original chronological order and show real LCD pixels. They are cropped, lightly brightened and silent; the screen animation is not composited onto the device. The full camera original remains on the Pixel and the transfer Mac. No microphone or speaker feature, physical knob launch, or reported actual token usage is demonstrated. Planned duration/token ranges are explicitly estimates.

## The browser checkpoints

1. Fix the requested read-only website review.
2. Open the HeySalad homepage and inspect its membership action.
3. Follow that action to `/join`; confirm it registers interest rather than activating membership.
4. Open `/help`; compare its navigation and support context.
5. Write three improvements: clarify the pilot action, preserve membership navigation in Help, and finish the support handoff. No forms were submitted.

## Firmware and recovery

The `openpromptlet-green-dot-v1-r180` build retains the accepted 180° rotation, existing ESP-NOW encoder receiver, board power behavior, and nonce-scoped USB host protocol. It adds the green activity dot and a five-checkpoint ring. The host's `VISUAL_STATE <nonce> <increasing-revision> <count-0-to-5>` updates visual progress only. It does not launch a task or grant approvals. A host heartbeat loss clears progress and shows the connection as offline.

ESP-IDF 5.3.2 compiled the installed image, SHA-256 `c3a7287464d5a9bdd852adec21c4633aaf4d4bc3144678083c97b341f58fcf54`. Windows matched the screen's USB identity and chip MAC, verified the existing partition table and original active image, and made a fresh full 16 MiB backup. The new image was written and verified in inactive `ota_1`; the old working action image stays in `ota_0`. Only the alternate OTA selection sector changed after image verification. No bootloader, NVS, sender firmware or partition table was written.

Backups and connection credentials remain private. The four knobs were accepted in the earlier firmware; their physical operation was not retested during this display recording.

## Sources and integration boundaries

[Waveshare's board documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46) lists the microphone, PCM5101 decoder and speaker. Their presence is not application-level voice acceptance. [Codex App Server](https://learn.chatgpt.com/docs/app-server) exposes plan updates, tool-item lifecycle and usage notifications; [Realtime VAD](https://developers.openai.com/api/docs/guides/realtime-vad) exposes speech start/stop events. Automatic app-server event mapping and onboard audio remain separate integration work. The recorded task used the current Codex computer-use driver and an explicit checkpoint adapter.

The installed display source is maintained on [`codex/green-dot-showcase-20261113`](https://github.com/heysalad-ai/openpromptlet/tree/codex/green-dot-showcase-20261113). The default branch retains its earlier buildable baseline while the new display profile receives broader hardware acceptance.
