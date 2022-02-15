#ifndef CELL_BATTLES_UTILS_H
#define CELL_BATTLES_UTILS_H

#include "SFML/Graphics.hpp"

template<class T>
sf::Vector2<T> clamp(sf::Vector2<T> value, T min, T max)
{
    if (value.x < min) value.x = min;
    else if (value.x > max) value.x = max;

    if (value.y < min) value.y = min;
    else if (value.y > max) value.y = max;

    return value;
}

template<class T>
sf::Vector2<T> clamp(sf::Vector2<T> value, sf::Vector2<T> min, sf::Vector2<T> max)
{
    if (value.x < min.x) value.x = min.x;
    else if (value.x > max.x) value.x = max.x;

    if (value.y < min.y) value.y = min.y;
    else if (value.y > max.y) value.y = max.y;

    return value;
}

template<class T>
T clamp(T value, T min, T max)
{
    if (value < min) value = min;
    else if (value > max) value = max;

    return value;
}

template<class T>
bool inBoundsIn(sf::Vector2<T> value, sf::Vector2<T> min, sf::Vector2<T> max)
{
    return !(value.x < min.x || value.x > max.x || value.y < min.y || value.y > max.y);
}

template<class T>
bool inBoundsEx(sf::Vector2<T> value, sf::Vector2<T> min, sf::Vector2<T> max)
{
    return !(value.x < min.x || value.x >= max.x || value.y < min.y || value.y >= max.y);
}

template<class T>
float lerp(T a, T b, float t)
{
    return a + (b - a) * t;
}

#endif //CELL_BATTLES_UTILS_H
