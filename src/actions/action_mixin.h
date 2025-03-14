#pragma once
#include <QDebug>

template<typename T>
class ActionsMixin : public T
{
public:

    explicit ActionsMixin(T* parent = nullptr)
        : T(parent)
    {}


protected:

};