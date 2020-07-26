// FUserInput.cpp : Defines the functions for the static library.
//
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "windows.h"
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN

#include <cstdint>
#include "UserInput.h"
namespace VKE
{
	namespace UserInput {

		void FUserInput::AddActionKeyPairToMap(const char* i_actionName, const EKeyCode& i_keycode)
		{
			m_actionKeyMap.insert({ i_actionName, i_keycode });
			if (!IsKeyExisted(i_keycode)) {
				m_keyStatusMap.insert({ i_keycode, false });
			}
		}

		void FUserInput::AddAxisKeyPairToMap(const char* i_axisName,const FAxisBrother& i_keycodes)
		{
			m_axisKeyMap.insert(
				{
					i_axisName,
					i_keycodes
				}
			);
			if (!IsKeyExisted(i_keycodes.negative_key)) {
				m_keyStatusMap.insert({ i_keycodes.negative_key, false });
			}
			if (!IsKeyExisted(i_keycodes.positive_key)) {
				m_keyStatusMap.insert({ i_keycodes.positive_key, false });
			}
		}

		FUserInput::~FUserInput()
		{
			m_actionBindings.clear();
			m_axisBindings.clear();

			m_actionKeyMap.clear();
			m_axisKeyMap.clear();

			m_keyStatusMap.clear();
		}

		bool FUserInput::IsKeyDown(const EKeyCode& i_key) const
		{
			// 1111 1111 1111 1110
			const short isKeyDownMask = ~1;
			// Reference: https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getkeystate
			auto keyState = GetAsyncKeyState(i_key);
			// if the high-order bit is 1, it is down
			bool isKeyDown = ((keyState & isKeyDownMask) != 0);
			return isKeyDown;
		}

		bool FUserInput::IsKeyExisted(const EKeyCode& i_key) const
		{
			return m_keyStatusMap.find(i_key) != m_keyStatusMap.end();
		}

		void FUserInput::UpdateInput()
		{
			// iterate through all action bindings
			{
				for (auto it = m_actionBindings.begin(); it != m_actionBindings.end(); ++it)
				{
					auto key = it->GetBoundKey();
					bool isKeyDown = IsKeyDown(key);

					// According to the input event type to do input check
					switch (it->GetInputType())
					{
					case IT_OnPressed:
						if (isKeyDown && !m_keyStatusMap[key]) {
							it->Execute();
						}
						break;
					case IT_OnReleased:
						if (!isKeyDown && m_keyStatusMap[key]) {
							it->Execute();
						}
						break;
					case IT_OnHold:
						if (isKeyDown) {
							it->Execute();
						}
						break;
					default:
						break;
					}

					// TODO:  More useful events should be implemented here later
					// ...
				}
			}
			// iterate through all axis bindings
			{
				for (auto it = m_axisBindings.begin(); it != m_axisBindings.end(); ++it)
				{
					auto keyL = it->GetBoundKey()[0];
					auto keyR = it->GetBoundKey()[1];

					bool isKeyDownL = IsKeyDown(keyL);
					bool isKeyDownR = IsKeyDown(keyR);

					// Currently only raw data
					float value = 0;
					if (isKeyDownL && !isKeyDownR) value = -1;
					if (!isKeyDownL && isKeyDownR) value = 1;

					it->Execute(value);
				}
			}

			// Update key state for binding keys
			for (auto it = m_keyStatusMap.begin(); it != m_keyStatusMap.end(); ++it)
			{
				it->second = IsKeyDown(it->first);
			}
		}

		bool FUserInput::IsActionNameValid(const char* i_actionName)
		{
			bool isValid = m_actionKeyMap.find(i_actionName) != m_actionKeyMap.end();
			return isValid;
		}

		bool FUserInput::IsAxisNameValid(const char* i_axisName)
		{
			bool isValid = m_axisKeyMap.find(i_axisName) != m_axisKeyMap.end();
			return isValid;
		}

		// -------------------------------
		// Action Binding Definition

		FActionBinding::FActionBinding(const char* i_actionName, const std::map<std::string, EKeyCode>& i_actionKeyMap, InputType i_inputType)
		{
			m_boundKeyCode = i_actionKeyMap.at(i_actionName);
			m_InputType = i_inputType;
			m_boundDelegate = nullptr;
		}

		FAxisBinding::FAxisBinding(const char* i_axisName, const std::map<std::string, FAxisBrother>& i_axisKeyMap)
		{
			m_boundKeyCode[0] = i_axisKeyMap.at(i_axisName).negative_key;
			m_boundKeyCode[1] = i_axisKeyMap.at(i_axisName).positive_key;
			m_boundDelegate = nullptr;
		}
	}
}

