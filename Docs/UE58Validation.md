# UE 5.8 validation

Verified on 2026-10-03 with a UE 5.8.0 source editor build, MSVC 14.44, Windows SDK 10.0.22621.0, and an NVIDIA RTX 3080:

| Check | Result |
| --- | --- |
| Win64 Development Editor build, non-unity ImGui module | Passed |
| NullRHI automation | 6/6 passed, zero warnings |
| D3D12 automation, including GPU pixel readback | 6/6 passed, zero warnings |
| D3D12 editor-hosted game viewport | 161 frames drawn, screenshot captured, normal exit |
| Vendored source, headers, license and documentation | All 17 files SHA256 match upstream `v1.92.9b` |

The [Slate screenshot](Evidence/UE58-ImGui-Slate.png) shows the demo, tables, dynamic 13/20/32 px fonts, and a registered Unreal checkerboard texture. It was visually inspected and its copied bytes verified by SHA256.

An independent Game target build was attempted with `-UsePrecompiled`. This local engine lacks the required Game manifests (including Launch and BuildSettings), so that target did not compile and packaged-game support remains unverified. The editor build and game viewport checks above passed independently.

The fixture in `Tests/UE58` loads this plugin in a minimal C++ project. Its C# rules are stored as `.cs.in` templates to prevent duplicate UBT rule discovery inside the plugin. The PowerShell runner stages the project outside the checkout, materializes the rules, and retains build logs, automation reports, and a Slate screenshot.

Run against an already built UE 5.8 Win64 editor:

```powershell
.\Scripts\ValidateUE58.ps1 -EnginePath 'F:\UnrealEngine'
```

Use `-SkipGPU` for a NullRHI run. Without that switch, the runner also uses D3D12, checks GPU pixel readback, and starts an offscreen game viewport showing the demo, tables, dynamic font sizes, and a registered Unreal texture. Inspect the generated `UE58-ImGui-Slate.png` after the run.

The `ImGui.Integration` automation group covers:

- Named keys and texture handle conversions, including direct `ImGui::Image(handle, size)` use.
- Fast keyboard/mouse press and release events, modifier keys, and resetting held inputs.
- Gamepad buttons, stick dead zones, and release after device disconnection.
- Explicit draw index/vertex offsets above 65,535 vertices, callbacks, and initialized Slate vertex fields.
- Dynamic texture creation, subregion updates, staged resource destruction, and RGBA upload snapshots. GPU runs additionally compare readback colors and unchanged pixels.
- Two contexts sharing an atlas, different DPI scales, dynamically sized fonts, and atlas reference cleanup.

The runner verifies the exported report instead of relying solely on the Unreal process exit code, which can be zero even when an automation test fails.

These checks cover the UE 5.8 Win64 editor and an editor-hosted game viewport. They do not verify packaged Development/Shipping builds, other engines or platforms, physical gamepad input, interactive multi-PIE sessions, or hot reload.

Draw callbacks run on the game thread while draw data is staged for Slate. The reset-render-state callback is a no-op because Slate owns the graphics state.
