#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс для обработки пользовательского ввода (мышь, окно)
class Hand
{
public:
    // Конструктор, принимает указатель на игровую доску
    Hand(Board* board) : board(board) {}

    // Основной метод для получения выбранной клетки от игрока
    // Возвращает кортеж из:
    // - Response (действие игрока)
    // - x-координата клетки (0-7 или -1 если не клетка)
    // - y-координата клетки (0-7 или -1 если не клетка)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;  // Событие SDL
        Response resp = Response::OK;  // Реакция по умолчанию
        int x = -1, y = -1;     // Абсолютные координаты курсора
        int xc = -1, yc = -1;   // Координаты клетки доски (0-7)

        while (true)  // Цикл обработки событий
        {
            if (SDL_PollEvent(&windowEvent))  // Проверяем наличие события
            {
                switch (windowEvent.type)  // Анализируем тип события
                {
                case SDL_QUIT:  // Событие закрытия окна
                    resp = Response::QUIT;
                    break;

                case SDL_MOUSEBUTTONDOWN:  // Клик мыши
                    x = windowEvent.motion.x;  // Получаем координаты клика
                    y = windowEvent.motion.y;

                    // Преобразуем в координаты клетки (0-7)
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // Проверка кликов на специальных кнопках:
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;  // Кнопка "Назад"
                    }
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;  // Кнопка "Повтор"
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;  // Клик по игровому полю
                    }
                    else
                    {
                        xc = -1;  // Клик вне значимых областей
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT:  // События окна
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();  // Обработка изменения размера
                        break;
                    }
                }

                if (resp != Response::OK)  // Если было значимое событие
                    break;  // Выходим из цикла
            }
        }
        return { resp, xc, yc };  // Возвращаем результат
    }

    // Метод ожидания действия пользователя (без привязки к клеткам)
    // Используется в меню и диалогах
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:  // Закрытие окна
                    resp = Response::QUIT;
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:  // Изменение размера
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN:  // Клик мыши
                {
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    // Проверяем только кнопку "Повтор" (специальная область)
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                break;
                }

                if (resp != Response::OK)
                    break;
            }
        }
        return resp;
    }

private:
    Board* board;  // Указатель на игровую доску для взаимодействия
};