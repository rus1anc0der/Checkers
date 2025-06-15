#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9; // Константа для представления "бесконечности" в алгоритме

/**
 * Класс Logic реализует игровую логику и ИИ для шашек.
 * Отвечает за:
 * - Поиск возможных ходов
 * - Оценку позиций
 * - Принятие решений для бота
 */

class Logic
{
public:
    /**
     * Инициализирует логику игры с привязкой к доске и настройкам.
     * @param board Указатель на игровое поле
     * @param config Указатель на конфигурацию игры
     */

    Logic(Board* board, Config* config) : board(board), config(config)
    {
        // Инициализация генератора случайных чисел (если не отключено в конфиге)
        rand_eng = std::default_random_engine(
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        // Загрузка настроек бота из конфигурации
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }


    /**
     * Находит оптимальные ходы для заданного цвета.
     * @param color Цвет фигур бота (false - белые, true - черные)
     * @return Вектор лучших ходов
     */
    vector<move_pos> find_best_turns(const bool color)
    {
        next_best_state.clear();
        next_move.clear();

        // Рекурсивный поиск лучшего хода
        find_first_best_turn(board->get_board(), color, -1, -1, 0);
        
        // Сборка последовательности ходов из состояний
        int cur_state = 0;
        vector<move_pos> res;
        do
        {
            res.push_back(next_move[cur_state]);
            cur_state = next_best_state[cur_state];
        } while (cur_state != -1 && next_move[cur_state].x != -1);
        return res;
    }

private:
    /**
     * Применяет ход к копии доски без изменения оригинала.
     * @param mtx Текущее состояние доски
     * @param turn Ход для применения
     * @return Новое состояние доски
     */
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        if (turn.xb != -1) // Если ход включает взятие фигуры
            mtx[turn.xb][turn.yb] = 0; // Удаляем взятую фигуру
        // Проверка на превращение в дамку
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        // Перемещение фигуры
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }


    /**
     * Вычисляет оценку позиции для заданного цвета.
     * @param mtx Состояние доски
     * @param first_bot_color Цвет бота
     * @return Численная оценка позиции
     */
    
    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        // Подсчет количества фигур каждого типа
        double w = 0, wq = 0, b = 0, bq = 0;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1); // Белые простые
                wq += (mtx[i][j] == 3); // Белые дамки
                b += (mtx[i][j] == 2); // Черные простые
                bq += (mtx[i][j] == 4); // Черные дамки
                // Дополнительная оценка потенциала фигур
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i); // Белые ближе к дамочному полю
                    b += 0.05 * (mtx[i][j] == 2) * (i); // Черные ближе к дамочному полю
                }
            }
        }
        // Корректировка оценки в зависимости от цвета бота
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // Проверка на победу/поражение
        if (w + wq == 0) return INF; // Противник не имеет фигур
        if (b + bq == 0) return 0; // Бот не имеет фигур

        // Коэффициенты для дамок
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        // Формула оценки: (наши фигуры + дамки*коэф) / (фигуры противника + дамки*коэф)
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // Рекурсивный поиск лучшего хода (основная логика)
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1)
    {
        next_best_state.push_back(-1);
        next_move.emplace_back(-1, -1, -1, -1);
        double best_score = -1;
        if (state != 0)
            find_turns(x, y, mtx);
        auto turns_now = turns;
        bool have_beats_now = have_beats;

        // Если нет взятий и это не начальное состояние, переходим к рекурсивному поиску
        if (!have_beats_now && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }

        vector<move_pos> best_moves;
        vector<int> best_states;

        // Перебор всех возможных ходов
        for (auto turn : turns_now)
        {
            size_t next_state = next_move.size();
            double score;
            if (have_beats_now)
            {
                // Продолжаем серию взятий
                score = find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, next_state, best_score);
            }
            else
            {
                // Обычный ход
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }
            // Обновление лучшего хода
            if (score > best_score)
            {
                best_score = score;
                next_best_state[state] = (have_beats_now ? int(next_state) : -1);
                next_move[state] = turn;
            }
        }
        return best_score;
    }

    // Рекурсивный поиск с альфа-бета отсечением
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1)
    {
        // База рекурсии - достигнута максимальная глубина
        if (depth == Max_depth)
        {
            return calc_score(mtx, (depth % 2 == color));
        }
        // Поиск возможных ходов для текущей позиции
        if (x != -1)
        {
            find_turns(x, y, mtx);
        }
        else
            find_turns(color, mtx);
        auto turns_now = turns;
        bool have_beats_now = have_beats;

        // Если нет взятий и это продолжение хода конкретной фигуры
        if (!have_beats_now && x != -1)
        {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        // Если нет возможных ходов
        if (turns.empty())
            return (depth % 2 ? 0 : INF);

        double min_score = INF + 1;
        double max_score = -1;
        // Перебор всех возможных ходов
        for (auto turn : turns_now)
        {
            double score = 0.0;
            if (!have_beats_now && x == -1)
            {
                // Обычный ход
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            else
            {
                // Продолжение серии ходов (для взятий)
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            // Обновление минимальной и максимальной оценки
            min_score = min(min_score, score);
            max_score = max(max_score, score);
            // Альфа-бета отсечение
            if (depth % 2)
                alpha = max(alpha, max_score);
            else
                beta = min(beta, min_score);
            if (optimization != "O0" && alpha >= beta)
                return (depth % 2 ? max_score + 1 : min_score - 1);
        }
        return (depth % 2 ? max_score : min_score);
    }

public:
    // Поиск всех возможных ходов для цвета
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // Поиск всех возможных ходов для конкретной фигуры
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // Поиск всех возможных ходов для цвета на заданной доске
    void find_turns(const bool color, const vector<vector<POS_T>>& mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        // Перебор всех клеток доски
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color) // Если фигура нужного цвета
                {
                    find_turns(i, j, mtx);
                    // Приоритет взятий
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        // Перемешивание ходов для разнообразия (если включено)
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // Поиск всех возможных ходов для конкретной фигуры на заданной доске
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>>& mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // Проверка возможных взятий
        switch (type)
        {
        case 1: // Белая простая
        case 2: // Черная простая
            // Проверка взятий для простых шашек
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default: // Дамки
            // Проверка взятий для дамок
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // Если есть взятия - возвращаем только их
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        // Проверка обычных ходов (если нет взятий)
        switch (type)
        {
        case 1: // Белая простая
        case 2: // Черная простая
            // Обычные ходы для простых шашек
        {
            POS_T i = ((type % 2) ? x - 1 : x + 1); // Направление движения
            for (POS_T j = y - 1; j <= y + 1; j += 2)
            {
                if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                    continue;
                turns.emplace_back(x, y, i, j);
            }
            break;
        }
        default: // Дамки
            // Обычные ходы для дамок
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

public:
    vector<move_pos> turns; // Доступные ходы
    bool have_beats; // Наличие взятий
    int Max_depth; // Глубина анализа

private:
    default_random_engine rand_eng; // ГСЧ
    string scoring_mode; // Стратегия оценки
    string optimization; // Уровень оптимизации
    vector<move_pos> next_move; // Последовательность ходов
    vector<int> next_best_state; // Состояния ИИ
    Board* board; // Игровое поле
    Config* config; // Настройки
};