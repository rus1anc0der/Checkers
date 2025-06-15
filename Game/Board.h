#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL.h>
#include <SDL_image.h>
#endif

using namespace std;

// Класс Board реализует игровое поле для шашек, включая:
//  - Визуализацию с использованием SDL
//  - Логику перемещения фигур
//  - Ведение истории ходов
//  - Визуальные эффекты (выделение клеток, подсветка ходов)
class Board
{
public:
    // Конструкторы
    Board() = default; // по умолчанию

    // Конструктор с указанием размеров окна
    Board(const unsigned int W, const unsigned int H) : W(W), H(H) {}

    // Основные public методы:

    // Инициализация и первичная отрисовка игрового поля
    int start_draw()
    {
        // Инициализация SDL
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }

        // Автоматический подбор размеров окна, если они не заданы
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desktop display mode");
                return 1;
            }
            // Создаём квадратное окно с небольшими отступами
            W = min(dm.w, dm.h);
            W -= W / 15;
            H = W;
        }

        // Создание основного окна
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }

        // Создание рендерера с оптимизацией
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }

        // Загрузка графических ресурсов
        board = IMG_LoadTexture(ren, board_path.c_str());         // Игровое поле
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str()); // Шашка белая
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str()); // Шашка чёрная
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str()); // Дамка белая
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str()); // Дамка чёрная
        back = IMG_LoadTexture(ren, back_path.c_str());           // Кнопка возврата
        replay = IMG_LoadTexture(ren, replay_path.c_str());       // Кнопка повтора

        // Проверка загрузки текстур
        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }

        // Получение фактических размеров области рендеринга
        SDL_GetRendererOutputSize(ren, &W, &H);

        // Инициализация начального состояния доски и первая отрисовка
        make_start_mtx(); // Начальная расстановка фигур
        rerender();       // Первичная отрисовка
        return 0;         // Успешная инициализация
    }

    // Сброс игры в начальное состояние
    void redraw()
    {
        game_results = -1;           // Сброс итога игры
        history_mtx.clear();         // Очистка истории ходов
        history_beat_series.clear(); // Очистка истории взятий
        make_start_mtx();            // Начальная расстановка
        clear_active();              // Сброс выделения
        clear_highlight();           // Сброс подсветки
    }

    // Перемещение фигуры с обработкой взятия (расширенная версия)
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        // Удаление побитой фигуры при наличии
        if (turn.xb != -1)
        {
            mtx[turn.xb][turn.yb] = 0;
        }
        // Основное перемещение
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);
    }

    // Основная логика перемещения фигуры
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        // Проверка допустимости хода
        if (mtx[i2][j2])
        {
            throw runtime_error("final position is not empty, can't move");
        }
        if (!mtx[i][j])
        {
            throw runtime_error("begin position is empty, can't move");
        }

        // Проверка превращения в дамку
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2; // 1->3 (белая дамка), 2->4 (черная дамка)

        // Выполнение перемещения
        mtx[i2][j2] = mtx[i][j];  // Перенос фигуры
        drop_piece(i, j);         // Очистка исходной позиции
        add_history(beat_series); // Сохранение в историю
    }

    // Удаление фигуры с доски
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0; // Очистка клетки
        rerender();    // Обновление отображения
    }

    // Превращение шашки в дамку
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        // Проверка возможности превращения
        if (mtx[i][j] == 0 || mtx[i][j] > 2)
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2; // Превращение (1->3, 2->4)
        rerender();     // Обновление отображения
    }

    // Получение текущего состояния доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx; // Возврат копии матрицы состояния
    }

    // Подсветка указанных клеток (для отображения возможных ходов)
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1; // Установка флага подсветки
        }
        rerender(); // Обновление отображения
    }

    // Очистка всех подсвеченных клеток
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0); // Сброс флагов подсветки
        }
        rerender(); // Обновление отображения
    }

    // Установка активной (выделенной) клетки
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x; // Сохранение координат
        active_y = y;
        rerender(); // Обновление отображения
    }

    // Сброс активной клетки
    void clear_active()
    {
        active_x = -1; // Сброс координат
        active_y = -1;
        rerender(); // Обновление отображения
    }

    // Проверка, подсвечена ли клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y]; // Возврат состояния подсветки
    }

    // Отмена последнего хода (откат)
    void rollback()
    {
        // Определение сколько ходов нужно откатить (с учетом серии взятий)
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();         // Удаление из истории
            history_beat_series.pop_back(); // Удаление информации о взятиях
        }
        mtx = *(history_mtx.rbegin()); // Восстановление состояния
        clear_highlight();             // Сброс подсветки
        clear_active();                // Сброс выделения
    }

    // Отображение результата игры
    void show_final(const int res)
    {
        game_results = res; // Сохранение результата
        rerender();         // Обновление отображения
    }

    // Обновление размеров окна
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H); // Получение новых размеров
        rerender();                             // Перерисовка
    }

    // Очистка ресурсов SDL
    void quit()
    {
        // Уничтожение всех текстур
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);

        // Уничтожение рендерера и окна
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);

        // Завершение работы SDL
        SDL_Quit();
    }

    // Деструктор (автоматическая очистка при уничтожении объекта)
    ~Board()
    {
        if (win)
            quit(); // Вызов метода очистки, если окно было создано
    }

private:
    // Добавление текущего состояния в историю
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);                 // Сохранение состояния доски
        history_beat_series.push_back(beat_series); // Сохранение информации о взятиях
    }

    // Создание начальной расстановки фигур
    void make_start_mtx()
    {
        // Очистка доски
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;
                // Расстановка черных шашек (верхние 3 ряда)
                if (i < 3 && (i + j) % 2 == 1)
                    mtx[i][j] = 2;
                // Расстановка белых шашек (нижние 3 ряда)
                if (i > 4 && (i + j) % 2 == 1)
                    mtx[i][j] = 1;
            }
        }
        add_history(); // Сохранение начального состояния в историю
    }

    // Основная функция рендеринга (перерисовки всего содержимого)
    void rerender()
    {
        // Очистка рендерера
        SDL_RenderClear(ren);

        // Отрисовка доски
        SDL_RenderCopy(ren, board, NULL, NULL);

        // Отрисовка всех фигур
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j])
                    continue; // Пропуск пустых клеток

                // Расчет позиции для отрисовки
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{wpos, hpos, W / 12, H / 12};

                // Выбор текстуры в зависимости от типа фигуры
                SDL_Texture *piece_texture;
                if (mtx[i][j] == 1)
                    piece_texture = w_piece; // Шашка белая
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece; // Шашка черная
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen; // Дамка белая
                else
                    piece_texture = b_queen; // Дамка черная

                // Отрисовка фигуры
                SDL_RenderCopy(ren, piece_texture, NULL, &rect);
            }
        }

        // Отрисовка подсветки возможных ходов (зеленые рамки)
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j])
                    continue;
                SDL_Rect cell{
                    int(W * (j + 1) / 10 / scale),
                    int(H * (i + 1) / 10 / scale),
                    int(W / 10 / scale),
                    int(H / 10 / scale)};
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // Отрисовка активной (выделенной) клетки (красная рамка)
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{
                int(W * (active_y + 1) / 10 / scale),
                int(H * (active_x + 1) / 10 / scale),
                int(W / 10 / scale),
                int(H / 10 / scale)};
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1); // Восстановление масштаба

        // Отрисовка кнопок управления
        SDL_Rect rect_left{W / 40, H / 40, W / 15, H / 15};
        SDL_RenderCopy(ren, back, NULL, &rect_left); // Кнопка "Назад"
        SDL_Rect replay_rect{W * 109 / 120, H / 40, W / 15, H / 15};
        SDL_RenderCopy(ren, replay, NULL, &replay_rect); // Кнопка "Повтор"

        // Отрисовка результата игры (если есть)
        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1)
                result_path = white_path; // Победа белых
            else if (game_results == 2)
                result_path = black_path; // Победа черных

            SDL_Texture *result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }
            SDL_Rect res_rect{W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5};
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        // Обновление экрана
        SDL_RenderPresent(ren);

        // Небольшая задержка и обработка событий
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Логирование ошибки
    void print_exception(const string &text)
    {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". " << SDL_GetError() << endl;
        fout.close();
    }

public:
    int W = 0; // Ширина окна
    int H = 0; // Высота окна

    // История состояний доски для отмены хода
    vector<vector<vector<POS_T>>> history_mtx;

private:
    // Указатели на SDL объекты
    SDL_Window *win = nullptr;   // Окно
    SDL_Renderer *ren = nullptr; // Рендерер

    // Текстуры:
    SDL_Texture *board = nullptr;   // Игровое поле
    SDL_Texture *w_piece = nullptr; // Шашка белая
    SDL_Texture *b_piece = nullptr; // Шашка черная
    SDL_Texture *w_queen = nullptr; // Дамка белая
    SDL_Texture *b_queen = nullptr; // Дамка черная
    SDL_Texture *back = nullptr;    // Кнопка Возврат
    SDL_Texture *replay = nullptr;  // Кнопка Повтора

    // Пути к файлам текстур
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";

    // Координаты активной клетки
    int active_x = -1, active_y = -1;

    // Результат игры:
    // если -1 игра продолжается
    // если 0 ничья
    // если 1 победа белых
    // если 2 победа черных
    int game_results = -1;

    // Матрица подсвеченных клеток (для отображения возможных ходов)
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));

    // Матрица состояния доски:
    // 0 - пусто
    // 1 - шашка белая
    // 2 - шашка черная
    // 3 - дамка белая
    // 4 - дамка черная
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));

    // История взятий (для правильной отмены хода)
    vector<int> history_beat_series;
};