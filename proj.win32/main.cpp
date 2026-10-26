#include "AppDelegate.h"
#include "platform/CCApplication.h"

#include <windows.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previousInstance, LPSTR commandLine, int showCommand)
{
    AppDelegate app;
    return cocos2d::Application::getInstance()->run();
}
