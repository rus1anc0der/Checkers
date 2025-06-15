#pragma once
#include <stdlib.h>

// хранение координат на поле
typedef int8_t POS_T;
// описываем ходы(перемещения) в игре
struct move_pos
{
    // начальная позиция
    POS_T x, y; // from
    // целевая позиция
    POS_T x2, y2; // to
    // позиция битой шашки
    // -1 значит не битая
    POS_T xb = -1, yb = -1; // beaten

    // обычный ход шашки
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2) : x(x), y(y), x2(x2), y2(y2)
    {
    }
    // взятие шашки (битой)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2, const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // сравниваем ходы
    // 2 хода равны если совпали начальные и конечные позиции
    bool operator==(const move_pos &other) const
    {
        return (x == other.x && y == other.y && x2 == other.x2 && y2 == other.y2);
    }
    // операция неравенства
    bool operator!=(const move_pos &other) const
    {
        return !(*this == other);
    }
};
