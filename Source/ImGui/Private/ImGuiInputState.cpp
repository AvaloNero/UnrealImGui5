// Distributed under the MIT License (MIT) (see accompanying LICENSE file)

#include "ImGuiInputState.h"

#include <algorithm>
#include <limits>
#include <type_traits>


FImGuiInputState::FImGuiInputState()
{
	Reset();
}

void FImGuiInputState::AddCharacter(TCHAR Char)
{
	InputCharacters.Add(Char);
}

void FImGuiInputState::SetKeyDown(uint32 KeyIndex, bool bIsDown)
{
	if (KeyIndex < Utilities::GetArraySize(KeysDown))
	{
		if (KeysDown[KeyIndex] != bIsDown)
		{
			KeysDown[KeyIndex] = bIsDown;
			KeyEvents.Add({ static_cast<ImGuiKey>(ImGuiKey_NamedKey_BEGIN + KeyIndex), bIsDown });
			KeysUpdateRange.AddPosition(KeyIndex);
		}
	}
}

void FImGuiInputState::SetMouseDown(uint32 MouseIndex, bool bIsDown)
{
	if (MouseIndex < Utilities::GetArraySize(MouseButtonsDown))
	{
		if (MouseButtonsDown[MouseIndex] != bIsDown)
		{
			MouseButtonsDown[MouseIndex] = bIsDown;
			MouseButtonEvents.Add({ static_cast<int>(MouseIndex), bIsDown });
			MouseButtonsUpdateRange.AddPosition(MouseIndex);
		}
	}
}

void FImGuiInputState::ClearUpdateState()
{
	ClearCharacters();
	KeyEvents.Reset();
	MouseButtonEvents.Reset();

	KeysUpdateRange.SetEmpty();
	MouseButtonsUpdateRange.SetEmpty();

	MouseWheelDelta = 0.f;

	bTouchProcessed = bTouchDown;
}

void FImGuiInputState::ClearCharacters()
{
	InputCharacters.Empty();
}

void FImGuiInputState::ClearKeys()
{
	for (uint32 Index = 0; Index < Utilities::GetArraySize(KeysDown); ++Index)
	{
		SetKeyDown(Index, false);
	}

	// Mark the whole array as dirty because potentially each entry could be affected.
	KeysUpdateRange.SetFull();
}

void FImGuiInputState::ClearMouseButtons()
{
	for (uint32 Index = 0; Index < Utilities::GetArraySize(MouseButtonsDown); ++Index)
	{
		SetMouseDown(Index, false);
	}

	// Mark the whole array as dirty because potentially each entry could be affected.
	MouseButtonsUpdateRange.SetFull();
}

void FImGuiInputState::ClearMouseAnalogue()
{
	MousePosition = FVector2D::ZeroVector;
	MouseWheelDelta = 0.f;
}

void FImGuiInputState::ClearModifierKeys()
{
	SetControlDown(false);
	SetShiftDown(false);
	SetAltDown(false);
	SetSuperDown(false);
}

void FImGuiInputState::SetModifierKey(bool& State, bool bIsDown, ImGuiKey Key)
{
	if (State != bIsDown)
	{
		State = bIsDown;
		KeyEvents.Add({ Key, bIsDown });
	}
}

void FImGuiInputState::ClearNavigationInputs()
{
	using std::fill;
	fill(NavigationInputs, &NavigationInputs[Utilities::GetArraySize(NavigationInputs)], 0.f);
}

