// Distributed under the MIT License (MIT) (see accompanying LICENSE file)

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ImGuiContextProxy.h"
#include "ImGuiDrawData.h"
#include "ImGuiInputState.h"
#include "ImGuiTextureHandle.h"
#include "Framework/Application/SlateApplication.h"
#include "RenderingThread.h"
#include "DynamicRHI.h"
#include "RHICommandList.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace
{
	constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FRestoreImGuiContext
	{
		ImGuiContext* Previous = ImGui::GetCurrentContext();
		~FRestoreImGuiContext() { ImGui::SetCurrentContext(Previous); }
	};

	struct FTestImGuiContext : FRestoreImGuiContext
	{
		ImGuiContext* Context = ImGui::CreateContext();
		FTestImGuiContext()
		{
			ImGui::SetCurrentContext(Context);
			ImGui::GetIO().IniFilename = nullptr;
			ImGui::GetIO().DisplaySize = ImVec2(640.f, 480.f);
			ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
		}
		~FTestImGuiContext() { ImGui::DestroyContext(Context); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImGuiKeyMappingTest, "ImGui.Integration.KeyMappingAndTextureHandles", TestFlags)
bool FImGuiKeyMappingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Named letter"), ImGuiInterops::ToImGuiKey(EKeys::Z), ImGuiKey_Z);
	TestEqual(TEXT("Navigation"), ImGuiInterops::ToImGuiKey(EKeys::PageDown), ImGuiKey_PageDown);
	TestEqual(TEXT("Function key"), ImGuiInterops::ToImGuiKey(EKeys::F12), ImGuiKey_F12);
	TestEqual(TEXT("Keypad"), ImGuiInterops::ToImGuiKey(EKeys::NumPadNine), ImGuiKey_Keypad9);
	TestEqual(TEXT("Unknown key"), ImGuiInterops::ToImGuiKey(FKey()), ImGuiKey_None);
	const TextureIndex TextureIndices[] = { INDEX_NONE, 0, 1, 1234 };
	for (TextureIndex Index : TextureIndices)
	{
		TestEqual(TEXT("Texture ID round trip"), ImGuiInterops::ToTextureIndex(ImGuiInterops::ToImTextureID(Index)), Index);
	}
	TestTrue(TEXT("Texture slot zero has a valid ImGui ID"), ImGuiInterops::ToImTextureID(0) != ImTextureID_Invalid);
	FImGuiTextureHandle Handle;
	TestEqual(TEXT("Null handle"), Handle.GetTextureId(), ImTextureID_Invalid);
	FTestImGuiContext Context;
	ImGui::NewFrame();
	ImGui::Begin("Texture handle API");
	ImGui::Image(Handle, ImVec2(16.f, 16.f)); // Verify the existing convenience API accepts ImTextureRef.
	ImGui::End();
	ImGui::EndFrame();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImGuiInputEventsTest, "ImGui.Integration.FastInputAndReset", TestFlags)
bool FImGuiInputEventsTest::RunTest(const FString& Parameters)
{
	FTestImGuiContext Context;
	FImGuiInputState Input;
	Input.SetKeyDown(EKeys::A, true);
	Input.SetKeyDown(EKeys::A, false);
	Input.SetMouseDown(EKeys::LeftMouseButton, true);
	Input.SetMouseDown(EKeys::LeftMouseButton, false);
	ImGuiInterops::CopyInput(ImGui::GetIO(), Input);
	Input.ClearUpdateState();
	ImGui::NewFrame();
	TestTrue(TEXT("Fast key press retained"), ImGui::IsKeyPressed(ImGuiKey_A, false));
	ImGui::EndFrame();
	bool bKeyReleased = false;
	bool bMouseClicked = false;
	bool bMouseReleased = false;
	for (int Frame = 0; Frame < 4; ++Frame)
	{
		ImGui::NewFrame();
		bKeyReleased |= ImGui::IsKeyReleased(ImGuiKey_A);
		bMouseClicked |= ImGui::IsMouseClicked(0);
		bMouseReleased |= ImGui::IsMouseReleased(0);
		ImGui::EndFrame();
	}
	TestTrue(TEXT("Fast key release retained"), bKeyReleased);
	TestTrue(TEXT("Fast mouse press retained"), bMouseClicked);
	TestTrue(TEXT("Fast mouse release retained"), bMouseReleased);
	Input.SetControlDown(true);
	Input.SetKeyDown(EKeys::LeftControl, true);
	Input.SetKeyDown(EKeys::B, true);
	ImGuiInterops::CopyInput(ImGui::GetIO(), Input);
	Input.ClearUpdateState();
	ImGui::NewFrame();
	TestTrue(TEXT("Control modifier"), ImGui::GetIO().KeyCtrl);
	TestTrue(TEXT("Held letter"), ImGui::IsKeyDown(ImGuiKey_B));
	ImGui::EndFrame();
	Input.ResetKeyboard();
	ImGuiInterops::CopyInput(ImGui::GetIO(), Input);
	ImGui::NewFrame();
	TestFalse(TEXT("Reset releases modifier"), ImGui::GetIO().KeyCtrl);
	TestFalse(TEXT("Reset releases held key"), ImGui::IsKeyDown(ImGuiKey_B));
	ImGui::EndFrame();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImGuiGamepadTest, "ImGui.Integration.GamepadMappingAndDisconnect", TestFlags)
bool FImGuiGamepadTest::RunTest(const FString& Parameters)
{
	ImGuiInterops::ImGuiTypes::FNavInputArray Inputs = {};
	ImGuiInterops::SetGamepadNavigationKey(Inputs, EKeys::Gamepad_FaceButton_Bottom, true);
	TestEqual(TEXT("Activate button"), Inputs[ImGuiKey_GamepadFaceDown - ImGuiKey_GamepadStart], 1.f);
	ImGuiInterops::SetGamepadNavigationAxis(Inputs, EKeys::Gamepad_LeftX, 1.f);
	TestEqual(TEXT("Full stick tilt"), Inputs[ImGuiKey_GamepadLStickRight - ImGuiKey_GamepadStart], 1.f);
	ImGuiInterops::SetGamepadNavigationAxis(Inputs, EKeys::Gamepad_LeftX, 0.1f);
	TestEqual(TEXT("Stick dead zone"), Inputs[ImGuiKey_GamepadLStickRight - ImGuiKey_GamepadStart], 0.f);
	FTestImGuiContext Context;
	ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_HasGamepad;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	ImGui::GetIO().AddKeyAnalogEvent(ImGuiKey_GamepadFaceDown, true, 1.f);
	ImGui::NewFrame();
	TestTrue(TEXT("Gamepad key held"), ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown));
	ImGui::EndFrame();
	FImGuiInputState DisconnectedInput;
	ImGuiInterops::CopyInput(ImGui::GetIO(), DisconnectedInput);
	ImGui::NewFrame();
	TestFalse(TEXT("Disconnect releases gamepad keys"), ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown));
	ImGui::EndFrame();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImGuiDrawOffsetsTest, "ImGui.Integration.DrawOffsetsAndCallbacks", TestFlags)
bool FImGuiDrawOffsetsTest::RunTest(const FString& Parameters)
{
	FTestImGuiContext Context;
	ImGui::NewFrame();
	ImDrawList Source(ImGui::GetDrawListSharedData());
	Source.VtxBuffer.resize(70004);
	FMemory::Memzero(Source.VtxBuffer.Data, Source.VtxBuffer.size_in_bytes());
	Source.VtxBuffer[0] = { ImVec2(20.f, 30.f), ImVec2(0.25f, 0.75f), IM_COL32(200, 100, 50, 255) };
	const ImDrawIdx SourceIndices[] = { 2, 1, 0, 0, 1, 2 };
	for (ImDrawIdx Index : SourceIndices) Source.IdxBuffer.push_back(Index);
	ImDrawCmd Command;
	Command.TexRef = ImTextureRef(ImGuiInterops::ToImTextureID(42));
	Command.ElemCount = 3;
	Source.CmdBuffer.push_back(Command);
	int CallbackCount = 0;
	Command.ElemCount = 0;
	Command.UserCallbackData = &CallbackCount;
	Command.UserCallback = [](const ImDrawList*, const ImDrawCmd* DrawCommand) { ++*static_cast<int*>(DrawCommand->UserCallbackData); };
	Source.CmdBuffer.push_back(Command);
	Command.UserCallback = nullptr;
	Command.ElemCount = 3;
	Command.IdxOffset = 3;
	Command.VtxOffset = 70000;
	Source.CmdBuffer.push_back(Command);
	FImGuiDrawList Staged;
	Staged.TransferDrawData(Source);
	const auto FinalCommand = Staged.GetCommand(2, FTransform2D());
	TestEqual(TEXT("Callback invoked once"), CallbackCount, 1);
	TestEqual(TEXT("Texture preserved"), FinalCommand.TextureId, 42);
	TestEqual(TEXT("Explicit index offset"), FinalCommand.IndexOffset, 3u);
	TArray<FSlateVertex> Vertices;
	Staged.CopyVertexData(Vertices, FTransform2D());
	TestEqual(TEXT("Slate vertex position"), Vertices[0].Position, FVector2f(20.f, 30.f));
	TestEqual(TEXT("Slate material UV initialized"), Vertices[0].MaterialTexCoords, FVector2f(0.25f, 0.75f));
	TestEqual(TEXT("Slate secondary color initialized"), Vertices[0].SecondaryColor, FColor::Transparent);
	TArray<SlateIndex> Indices;
	Staged.CopyIndexData(Indices, FinalCommand.IndexOffset, FinalCommand.NumElements, FinalCommand.VertexOffset);
	if (sizeof(SlateIndex) >= sizeof(uint32)) TestEqual(TEXT("Large vertex offset preserved"), static_cast<uint32>(Indices[2]), 70002u);
	ImGui::EndFrame();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImGuiTextureLifecycleTest, "ImGui.Integration.DynamicTextureLifecycle", TestFlags)
bool FImGuiTextureLifecycleTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized()) { AddError(TEXT("Slate is required for texture validation.")); return false; }
	FTextureManager Manager;
	ImTextureData Texture;
	Texture.Create(ImTextureFormat_RGBA32, 4, 4);
	for (int Pixel = 0; Pixel < 16; ++Pixel)
	{
		Texture.Pixels[Pixel * 4 + 0] = 200;
		Texture.Pixels[Pixel * 4 + 1] = 80;
		Texture.Pixels[Pixel * 4 + 2] = 20;
		Texture.Pixels[Pixel * 4 + 3] = 255;
	}
	ImVector<ImTextureData*> Textures;
	Textures.push_back(&Texture);
	ImDrawData DrawData;
	DrawData.Textures = &Textures;
	Manager.UpdateImGuiTextures(DrawData);
	TestEqual(TEXT("Texture created"), Texture.Status, ImTextureStatus_OK);
	const ImTextureID TextureId = Texture.GetTexID();
	TestTrue(TEXT("Nonzero texture ID"), TextureId != ImTextureID_Invalid);
	const bool bReadGPU = GDynamicRHI && !FString(GDynamicRHI->GetName()).StartsWith(TEXT("Null"));
	const auto ReadPixels = [&Texture]()
	{
		FlushRenderingCommands();
		const FTextureRHIRef Resource = static_cast<UTexture2D*>(Texture.BackendUserData)->GetResource()->TextureRHI;
		TArray<FColor> Pixels;
		ENQUEUE_RENDER_COMMAND(ImGuiTestReadback)([Resource, &Pixels](FRHICommandListImmediate& Commands)
		{
			FReadSurfaceDataFlags Flags(RCM_UNorm);
			Flags.SetLinearToGamma(false);
			Commands.ReadSurfaceData(Resource, FIntRect(0, 0, 4, 4), Pixels, Flags);
		});
		FlushRenderingCommands();
		return Pixels;
	};
	// Deliberately change ImGui's CPU data before the queued upload executes.
	FMemory::Memzero(Texture.Pixels, Texture.GetSizeInBytes());
	if (bReadGPU)
	{
		const TArray<FColor> Pixels = ReadPixels();
		if (TestEqual(TEXT("GPU texture pixel count"), Pixels.Num(), 16))
		{
			TestEqual(TEXT("Creation snapshots RGBA pixels correctly"), Pixels[0], FColor(200, 80, 20, 255));
		}
	}
	Texture.UpdateRect = { 1, 1, 2, 2 };
	for (int Y = 1; Y < 3; ++Y)
	{
		for (int X = 1; X < 3; ++X)
		{
			uint8* Pixel = Texture.Pixels + (X + Y * Texture.Width) * 4;
			Pixel[0] = 40; Pixel[1] = 100; Pixel[2] = 220; Pixel[3] = 255;
		}
	}
	Texture.SetStatus(ImTextureStatus_WantUpdates);
	Manager.UpdateImGuiTextures(DrawData);
	TestEqual(TEXT("Texture updated"), Texture.Status, ImTextureStatus_OK);
	TestEqual(TEXT("Update preserves texture ID"), Texture.GetTexID(), TextureId);
	FMemory::Memzero(Texture.Pixels, Texture.GetSizeInBytes());
	if (bReadGPU)
	{
		const TArray<FColor> Pixels = ReadPixels();
		if (TestEqual(TEXT("Updated GPU texture pixel count"), Pixels.Num(), 16))
		{
			TestEqual(TEXT("Partial update preserves other pixels"), Pixels[0], FColor(200, 80, 20, 255));
			TestEqual(TEXT("Partial update snapshots RGBA pixels correctly"), Pixels[5], FColor(40, 100, 220, 255));
		}
	}
	else AddInfo(TEXT("GPU pixel readback skipped with NullRHI."));
	Texture.WantDestroyNextFrame = true;
	Texture.SetStatus(ImTextureStatus_WantDestroy);
	Texture.UnusedFrames = 1;
	Manager.UpdateImGuiTextures(DrawData);
	TestEqual(TEXT("Staged draws retain texture"), Texture.Status, ImTextureStatus_WantDestroy);
	Texture.UnusedFrames = 3;
	Manager.UpdateImGuiTextures(DrawData);
	TestEqual(TEXT("Texture destroyed"), Texture.Status, ImTextureStatus_Destroyed);
	TestEqual(TEXT("ID invalidated"), Texture.GetTexID(), ImTextureID_Invalid);
	FlushRenderingCommands();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImGuiSharedAtlasTest, "ImGui.Integration.SharedAtlasAndDPI", TestFlags)
bool FImGuiSharedAtlasTest::RunTest(const FString& Parameters)
{
	FRestoreImGuiContext Restore;
	FTextureManager Manager;
	ImFontAtlas Atlas;
	Atlas.AddFontDefault();
	{
		FImGuiContextProxy First(TEXT("ImGuiTest_First"), -1, &Atlas, 1.f, Manager);
		FImGuiContextProxy Second(TEXT("ImGuiTest_Second"), -1, &Atlas, 2.f, Manager);
		TestEqual(TEXT("Shared atlas users"), Atlas.RefCount, 2);
		First.SetAsCurrent();
		ImGui::GetForegroundDrawList()->AddText(ImVec2(10.f, 10.f), IM_COL32_WHITE, "First shared atlas");
		ImGui::Begin("First context"); ImGui::TextUnformatted("Shared fonts at 1x"); ImGui::End();
		First.Tick(1.f / 60.f);
		Second.SetAsCurrent();
		TestEqual(TEXT("Second context DPI"), ImGui::GetStyle().FontScaleDpi, 2.f);
		ImGui::Begin("Second context");
		ImGui::PushFont(nullptr, 32.f);
		ImGui::TextUnformatted("Dynamic fonts at 2x");
		ImGui::GetForegroundDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(10.f, 10.f), IM_COL32_WHITE, "Second shared atlas");
		ImGui::PopFont();
		ImGui::End();
		Second.Tick(1.f / 60.f);
		TestTrue(TEXT("First context produced draw data"), !First.GetDrawData().IsEmpty());
		TestTrue(TEXT("Second context produced draw data"), !Second.GetDrawData().IsEmpty());
		TestTrue(TEXT("Shared atlas uploaded"), Atlas.TexRef.GetTexID() != ImTextureID_Invalid);
	}
	TestEqual(TEXT("Contexts release shared atlas"), Atlas.RefCount, 0);
	FlushRenderingCommands();
	return true;
}
#endif
