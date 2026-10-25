# Reference review

User-supplied reference: https://github.com/alxv2016/folloup-sticky/tree/folloup-waveshare

Inspected branch commit: `f0b56e969ba3cf7a9d1bed0a77472337abd07ee4`.

Read its README, app architecture, Gemini-service notes, Wi-Fi provisioning discussion, and component structure. Followup emphasizes recording-first capture, SD archives, Gemini transcription, and distinct UI/services. Its code is GPL-3.0. No Followup source or art was copied into Wisp; it was used as a behavior/architecture reference. The reference repository was not built or modified.

Wisp 0.2 applies those ideas with its own app-grid routing, separate Notes/Tasks, storage and provisioning helpers, background transcription worker, and SD library. Direct Gemini uses the documented generateContent API and a Google-published CA bundle. The optional existing local Whisper bridge remains available.

The reference's documented scope does not establish EPUB support. Wisp's EPUB conversion is independently implemented in the browser, with fflate 0.8.2 for ZIP decompression, package/spine-order parsing, and TXT copies for the board's reader.

The board remains on the existing official Waveshare Arduino display/audio drivers. Conflicting power-management descriptions in third-party/reference material were not used to change PMIC configuration without physical-board evidence.

The user asked to simplify the pet during implementation. The final design is a small original pixel cat, not the larger illustrated character and not a copied ChatGPT pet asset.

In 0.2.2 the user selected concept 3, Pocket Ghost. It is implemented as two original 24x24 one-bit poses in native drawing code and matching browser SVG.
