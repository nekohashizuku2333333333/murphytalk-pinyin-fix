#ifndef MURPHYTALK_QWINDOWSYSTEM_QWS_COMPAT_H
#define MURPHYTALK_QWINDOWSYSTEM_QWS_COMPAT_H

class QWSServer
{
public:
	class KeyboardFilter
	{
	public:
		virtual ~KeyboardFilter() {}
		virtual bool filter(int unicode, int keycode, int modifiers, bool isPress, bool autoRepeat) = 0;
	};

	void setKeyboardFilter(KeyboardFilter *);
	void sendKeyEvent(int unicode, int keycode, int modifiers, bool isPress, bool autoRepeat);
};

extern QWSServer *qwsServer;

#endif
