#pragma once

#include "CppUnitTest.h"

#include <collection.h>
#include <ppltasks.h>

#include "Await.h"

class Log : public std::wostringstream
{
public:
    Log()
    {
    }

    ~Log()
    {
        *this << std::endl;
        ::Microsoft::VisualStudio::CppUnitTestFramework::Logger::WriteMessage(this->str().c_str());
    }
};
