#include "MouseState.h"

MouseState MouseState::_prevState;

MouseState::MouseState() 
	: _x(0), _y(0), _dx(0), _dy(0)
{
}

MouseState::~MouseState()
{
}

int MouseState::GetX()
{
	return _x;
}
	
int MouseState::GetY()
{
	return _y;
}

int MouseState::GetDX()
{
	return _dx;
}

int MouseState::GetDY()
{
	return _dy;
}

bool MouseState::IsOverWindow()
{
	return _overWindow;
}

bool MouseState::IsJustOverWindow()
{
	return _overWindow && !_prevState._overWindow;
}

ButtonState MouseState::GetButtonState(MouseButtons button)
{
	return _buttonStates[button];
}

bool MouseState::IsButtonDown(MouseButtons button)
{
	return _buttonStates[button].Pressed;
}

bool MouseState::IsButtonUp(MouseButtons button)
{
	return !_buttonStates[button].Pressed;
}

bool MouseState::IsButtonJustPressed(MouseButtons button)
{
	return _buttonStates[button].JustPressed;
}

bool MouseState::IsButtonJustReleased(MouseButtons button)
{
	return _buttonStates[button].JustReleased;
}

void MouseState::SetCursorPosition(int x, int y, HWND hwnd)
{
	HRESULT hr;

    POINT pos;
    pos.x = x;
    pos.y = y;

	if (hwnd)
	{
		V_WIN(ClientToScreen(hwnd, &pos));
	}

    V_WIN(SetCursorPos(pos.x, pos.y));

	_prevState._x = x;
	_prevState._y = y;
}

void MouseState::SetCursorVisible(bool visible)
{
	HRESULT hr;

	V_WIN(ShowCursor(visible));
}

MouseState MouseState::GetState(HWND hwnd)
{
	HRESULT hr;

	POINT pos;	
	V_WIN(GetCursorPos(&pos));
	
	// If a window was supplied, transform the point to window space
	if (hwnd)
	{
		V_WIN(ScreenToClient(hwnd, &pos));
	}

	MouseState newState;
	newState._x = pos.x;
	newState._y = pos.y;
	newState._dx = pos.x - _prevState._x;
	newState._dy = pos.y - _prevState._y;

	newState._buttonStates[LeftButton].Pressed = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
	newState._buttonStates[LeftButton].JustPressed = 
		newState._buttonStates[LeftButton].Pressed && !_prevState._buttonStates[LeftButton].Pressed;
	newState._buttonStates[LeftButton].JustReleased = 
		!newState._buttonStates[LeftButton].Pressed && _prevState._buttonStates[LeftButton].Pressed;

	newState._buttonStates[RightButton].Pressed = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
	newState._buttonStates[RightButton].JustPressed = 
		newState._buttonStates[RightButton].Pressed && !_prevState._buttonStates[RightButton].Pressed;
	newState._buttonStates[RightButton].JustReleased = 
		!newState._buttonStates[RightButton].Pressed && _prevState._buttonStates[RightButton].Pressed;

	newState._buttonStates[CenterButton].Pressed = (GetKeyState(VK_MBUTTON) & 0x8000) != 0;
	newState._buttonStates[CenterButton].JustPressed = 
		newState._buttonStates[CenterButton].Pressed && !_prevState._buttonStates[CenterButton].Pressed;
	newState._buttonStates[CenterButton].JustReleased = 
		!newState._buttonStates[CenterButton].Pressed && _prevState._buttonStates[CenterButton].Pressed;

	newState._buttonStates[Mouse4].Pressed = (GetKeyState(VK_XBUTTON1) & 0x8000) != 0;
	newState._buttonStates[Mouse4].JustPressed = 
		newState._buttonStates[Mouse4].Pressed && !_prevState._buttonStates[Mouse4].Pressed;
	newState._buttonStates[Mouse4].JustReleased = 
		!newState._buttonStates[Mouse4].Pressed && _prevState._buttonStates[Mouse4].Pressed;

	newState._buttonStates[Mouse5].Pressed = (GetKeyState(VK_XBUTTON2) & 0x8000) != 0;
	newState._buttonStates[Mouse5].JustPressed = 
		newState._buttonStates[Mouse5].Pressed && !_prevState._buttonStates[Mouse5].Pressed;
	newState._buttonStates[Mouse5].JustReleased = 
		!newState._buttonStates[Mouse5].Pressed && _prevState._buttonStates[Mouse5].Pressed;

	if (hwnd)
	{
		RECT clientArea;
		V_WIN(GetClientRect(hwnd, &clientArea));

		newState._overWindow = (pos.x >= 0 && pos.x < clientArea.right && pos.y >= 0 && pos.y < clientArea.bottom);
	}
	else 
	{
		newState._overWindow = false;
	}

	_prevState = newState;
	return _prevState;
}

