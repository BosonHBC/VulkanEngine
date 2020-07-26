#pragma once
#include <vector>
#include <map>
#include "InputDelegate.h"

#define IT_OnPressed 1
#define IT_OnReleased 2
#define IT_OnHold 3

struct GLFWwindow;
namespace VKE {
	namespace UserInput {
		enum EKeyCode
		{
			Left = 0x25,
			Up = 0x26,
			Right = 0x27,
			Down = 0x28,

			Space = 0x20,

			Escape = 0x1b,

			Shift = 0x10,
			Control = 0x11,
			Alt = 0x12,

			Tab = 0x09,
			CapsLock = 0x14,

			BackSpace = 0x08,
			Enter = 0x0d,
			Delete = 0x2e,

			PageUp = 0x21,
			PageDown = 0x22,
			End = 0x23,
			Home = 0x24,

			F1 = 0x70,
			F2 = 0x71,
			F3 = 0x72,
			F4 = 0x73,
			F5 = 0x74,
			F6 = 0x75,
			F7 = 0x76,
			F8 = 0x77,
			F9 = 0x78,
			F10 = 0x79,
			F11 = 0x7a,
			F12 = 0x7b,

			A = 0x41,
			B = 0x42,
			C = 0x43,
			D = 0x44,
			E = 0x45,
			F = 0x46,
			G = 0x47,
			H = 0x48,
			I= 0x49,
			J = 0x4A,
			K = 0x4B,
			L = 0x4C,
			M = 0x4D,
			N = 0x4E,
			O = 0x4F,
			P = 0x50,
			Q = 0x51,
			R = 0x52,
			S = 0x53,
			T = 0x54,
			U = 0x55,
			V = 0x56,
			W = 0x57,
			X = 0x58,
			Y = 0x59,
			Z = 0x5A,
		};
		struct FAxisBrother {
			FAxisBrother(EKeyCode i_nKey, UserInput::EKeyCode i_pKey) : negative_key(i_nKey), positive_key(i_pKey) {}
			EKeyCode negative_key;
			EKeyCode positive_key;
		};

		// -------------------------------
		// Input Type
		// 5 common Input types for keyboard
		enum EInputType
		{
			EIT_OnPressed = 1,
			EIT_OnReleased = 2,
			EIT_OnHold = 3,
		};
		typedef uint8_t InputType;

		// -------------------------------
		// Action Binding
		// Action Binding struct stores binding info like axis name and the delegate function
		struct FActionBinding {
		public:
			/** Constructor for FActionBinding struct*/
			FActionBinding()
				:m_boundKeyCode(), m_InputType(IT_OnPressed), m_boundDelegate(nullptr)
			{}

			FActionBinding(const char* i_actionName, const std::map<std::string, EKeyCode>& i_actionKeyMap, InputType i_inputType);

			/** Execute the binding functions*/
			void Execute() { m_boundDelegate->Execute(); }

			/** Setters */
			void SetDelegate(IDelegateHandler<>* i_boundDelegate) { m_boundDelegate = i_boundDelegate; }
			/** Getters */
			EKeyCode GetBoundKey() const { return m_boundKeyCode; }
			InputType GetInputType() const { return m_InputType; }
		private:
			EKeyCode m_boundKeyCode;
			InputType m_InputType;
			// Delegate with no input parameter
			IDelegateHandler<>* m_boundDelegate;
		};

		// -------------------------------
		// Axis Binding
		// Axis Binding struct stores binding info like axis name and the delegate function
		struct FAxisBinding {
			/** Default Constructor for FAxisBinding struct*/
			FAxisBinding()
				:m_boundKeyCode(), m_boundDelegate(nullptr)
			{}

			FAxisBinding(const char* i_axisName, const std::map<std::string, FAxisBrother>& i_axisKeyMap);

			/** Execute the binding functions*/
			void Execute(float i_axisValue) { m_boundDelegate->Execute(i_axisValue); }

			/** Setters */
			void SetDelegate(IDelegateHandler<float>* i_boundDelegate) { m_boundDelegate = i_boundDelegate; }
			/** Getters */
			const EKeyCode* GetBoundKey() const { return m_boundKeyCode; }

		private:
			EKeyCode m_boundKeyCode[2];
			// Delegate with one float parameter
			IDelegateHandler<float>* m_boundDelegate;
		};

		// -------------------------------
		// Advanced UserInput
		// Advanced UserInput struct helps user use input tools
		struct FUserInput
		{

		public:
			// ---------------------
			/* Constructor with mappings*/
			FUserInput() : m_actionBindings(), m_actionKeyMap(), m_axisBindings(), m_axisKeyMap()
			{}
			FUserInput(std::map<std::string, EKeyCode>& iActionKeyMap, std::map<std::string, FAxisBrother>& iAxisKeyMap) : m_actionKeyMap(iActionKeyMap), m_axisKeyMap(iAxisKeyMap) {}
			FUserInput(const FUserInput& i_other) = delete;
			FUserInput& operator = (const FUserInput& i_other) = delete;
			FUserInput(const FUserInput&& i_other) = delete;
			FUserInput& operator = (const FUserInput&& i_other) = delete;
			~FUserInput();
			/** Add action key pair to map*/
			void AddActionKeyPairToMap(const char* i_actionName, const EKeyCode& i_keycode);
			/** Add action key pair to map*/
			void AddAxisKeyPairToMap(const char* i_axisName, const FAxisBrother& i_keycodes);

			// ---------------------
			/** Core update function to check and update binding maps. Should be called in a Tick function*/
			void UpdateInput();

#pragma region BindAction
			/** Bind an action to a [void] static / global function with no input parameter
			* i_actionName: the name of the action in data table
			* i_inputType: the input event
			* i_func: function to bound
			*/
			bool BindAction(const char* i_actionName, const InputType i_inputType, void(*i_func)())
			{
				bool result = true;
				if (!(result = IsActionNameValid(i_actionName) ? true : false)) {
					// Invalid action name
					result = false;
					printf("The action name: [%s] doesn't exist in the data", i_actionName);
					return result;
				}
				FActionBinding newActionBinding = FActionBinding(i_actionName, m_actionKeyMap, i_inputType);
				IDelegateHandler<>* boundDelegate = StaticFuncDelegate<>::CreateStaticDelegate(i_func);
				newActionBinding.SetDelegate(boundDelegate);
				m_actionBindings.push_back(newActionBinding);
				return result;
			}

			/** Bind an action to a [void] member function with no input parameter
			* i_actionName: the name of the action in data table
			* i_inputType: the input event
			* i_ptr: the instance of the template class
			* i_func: function to bound
			*/
			template<class T>
			bool BindAction(const char* i_actionName, const InputType i_inputType, T* i_ptr, void(T::* i_func)())
			{
				bool result = true;
				if (!(result = IsActionNameValid(i_actionName) ? true : false)) {
					// Invalid action name
					result = false;
					printf("The action name: [%s] doesn't exist in the data", i_actionName);
					return result;
				}
				FActionBinding newActionBinding = FActionBinding(i_actionName, m_actionKeyMap, i_inputType);
				IDelegateHandler<>* boundDelegate = MemberFuncDelegate<T>::CreateMemberDelegate(i_ptr, i_func);
				newActionBinding.SetDelegate(boundDelegate);
				m_actionBindings.push_back(newActionBinding);
				return result;
			}

#pragma endregion

#pragma region BindAxis
			/** Bind an action to a [void] static / global function with a float parameter
			* i_axisName: the name of the axis in data table
			* i_func: function to bound, the input parameter should be a float
			*/
			bool BindAxis(const char* i_axisName, void(*i_func)(float)) {
				bool result = true;
				if (!(result = IsAxisNameValid(i_axisName) ? true : false)) {
					// Invalid axis name
					result = false;
					printf("The axis name: [%s] doesn't exist in the data", i_axisName);
					return result;
				}
				FAxisBinding newAxisBinding = FAxisBinding(i_axisName, m_axisKeyMap);
				IDelegateHandler<float>* boundDelegate = StaticFuncDelegate<float>::CreateStaticDelegate(i_func);
				newAxisBinding.SetDelegate(boundDelegate);
				m_axisBindings.push_back(newAxisBinding);
				return result;
			}

			/** Bind an action to a [void] member function with with a float parameter
			* i_actionName: the name of the axis in data table
			* i_ptr: the instance of the template class
			* i_func: function to bound, the input parameter should be a float
			*/
			template<class T>
			bool BindAxis(const char* i_axisName, T* i_ptr, void(T::* i_func)(float))
			{
				bool result = true;
				if (!(result = IsAxisNameValid(i_axisName) ? true : false)) {
					// Invalid axis name
					result = false;
					printf("The axis name: [%s] doesn't exist in the data", i_axisName);
					return result;
				}
				FAxisBinding newAxisBinding = FAxisBinding(i_axisName, m_axisKeyMap);
				IDelegateHandler<float>* boundDelegate = MemberFuncDelegate<T, float>::CreateMemberDelegate(i_ptr, i_func);
				newAxisBinding.SetDelegate(boundDelegate);
				m_axisBindings.push_back(newAxisBinding);
				return result;
			}
#pragma endregion
		private:
			// ---------------------
			// Action bindings
			/** List of action bindings*/
			std::vector<FActionBinding> m_actionBindings;
			/** Action Name to keyCode map*/
			std::map<std::string, EKeyCode> m_actionKeyMap;

			// ---------------------
			// Axis bindings
			/** List of action bindings*/
			std::vector<FAxisBinding> m_axisBindings;
			/** Axis Name to keyCodes map*/
			std::map<std::string, FAxisBrother> m_axisKeyMap;

			// ---------------------
			// Helper utilities
			/** Check if key down*/
			bool IsKeyDown(const EKeyCode& i_key) const;

			/** Check if key codes already exist in key status map */
			bool IsKeyExisted(const EKeyCode& i_key) const;

			/** Check if the input action name is a valid axis*/
			bool IsActionNameValid(const char* i_actionName);

			/** Check if the input axis name is a valid axis*/
			bool IsAxisNameValid(const char* i_axisName);

			/** record the outdated key code status map, true -> keyDown*/
			std::map< EKeyCode, bool> m_keyStatusMap;
		};

	}
}