#ifndef MURPHYTALK_QWINDOWSYSTEM_QWS_COMPAT_H
#define MURPHYTALK_QWINDOWSYSTEM_QWS_COMPAT_H

class QWSServer
{
public:
	class KeyboardFilter
	{
	public:
		virtual bool filter(int unicode, int keycode, int modifiers, bool isPress, bool autoRepeat) = 0;
	};

	static void setKeyboardFilter(KeyboardFilter *);
	static void sendKeyEvent(int unicode, int keycode, int modifiers, bool isPress, bool autoRepeat);
};

#endif
