#pragma once
#include <QtConcurrent/QtConcurrent>
#include <functional>

template<typename Func, typename... Args>
inline QFuture<void> fireAndForget(Func&& f, Args&&... args) {
    return QtConcurrent::run(std::forward<Func>(f), std::forward<Args>(args)...);
}
