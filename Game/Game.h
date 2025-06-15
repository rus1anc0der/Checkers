#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

/**
 * Основной класс, управляющий игровым процессом в шашках.
 * Координирует взаимодействие между игроком/ботом, игровой логикой и отображением.
 */
 
class Game
{
  public:
      /**
     * Инициализирует игровые компоненты и очищает лог-файл.
     */
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }
    /**
     * Запускает и управляет игровым процессом.
     * @return Код результата игры (0 - выход, 1 - победа черных, 2 - ничья)
     */
    int play()
    {
        // Засекаем время начала игры 
        auto start = chrono::steady_clock::now();

        // Инициализация режима игры
        if (is_replay) {
            logic = Logic(&board, &config);  // Пересоздаем логику игры
            config.reload();                // Перезагружаем конфигурацию
            board.redraw();                  // Перерисовываем доску
        }
        else {
            board.start_draw();              // Начальная отрисовка доски для новой игры
        }
        is_replay = false;                  // Сбрасываем флаг реплея

        int turn_num = -1;                  // Номер хода (-1 так как сначала ++)
        bool is_quit = false;               // Флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns"); // Макс. число ходов из конфига

        // Главный игровой цикл
        while (++turn_num < Max_turns) {
            beat_series = 0;  // Сбрасываем счетчик серии взятий

            // Находим возможные ходы для текущего игрока (0 - белые, 1 - черные)
            logic.find_turns(turn_num % 2);

            // Если ходов нет - игра завершается
            if (logic.turns.empty()) break;

            // Устанавливаем уровень сложности бота для текущего игрока
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));

            // Если текущий игрок - человек (не бот)
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot"))) {
                auto resp = player_turn(turn_num % 2);  // Обрабатываем ход игрока

                if (resp == Response::QUIT) {   // Выход из игры
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY) {  // Запрос на реплей
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK) {    // Отмена хода
                    // Особые условия отмены при игре против бота
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2) {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series) --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else {
                bot_turn(turn_num % 2);  // Ход бота
            }
        }

        // Замер времени игры и запись в лог
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // Обработка завершения игры
        if (is_replay) return play();  // Рестарт игры
        if (is_quit) return 0;         // Выход без результата

        // Определение результата игры
        int res = 2;  // По умолчанию ничья (2)
        if (turn_num == Max_turns) {
            res = 0;  // Ничья по достижению лимита ходов
        }
        else if (turn_num % 2) {
            res = 1;  // Победа черных
        }

        // Показ финального экрана и обработка ответа игрока
        board.show_final(res);
        auto resp = hand.wait();
        if (resp == Response::REPLAY) {
            is_replay = true;
            return play();  // Рестарт по запросу игрока
        }
        return res;  // Возврат результата игры
    }

  private:
      void bot_turn(const bool color)
      {
          // Засекаем время начала хода бота для последующего замера производительности
          auto start = chrono::steady_clock::now();

          // Получаем задержку для бота из конфигурации (в миллисекундах)
          auto delay_ms = config("Bot", "BotDelayMS");

          // Создаем отдельный поток для задержки, чтобы основной поток мог выполнять вычисления
          thread th(SDL_Delay, delay_ms);

          // Находим лучшие ходы для бота на основе текущего состояния доски и цвета фигур
          auto turns = logic.find_best_turns(color);

          // Ожидаем завершения потока с задержкой, чтобы задержка была одинаковой для каждого хода
          th.join();

          bool is_first = true;

          // Выполняем найденные ходы
          for (auto turn : turns)
          {
              // Добавляем задержку перед каждым ходом, кроме первого
              if (!is_first)
              {
                  SDL_Delay(delay_ms);
              }
              is_first = false;

              // Увеличиваем счетчик серии ударов, если ход является ударным (xb != -1)
              beat_series += (turn.xb != -1);

              // Перемещаем фигуру на доске в соответствии с текущим ходом
              board.move_piece(turn, beat_series);
          }

          // Засекаем время окончания хода бота
          auto end = chrono::steady_clock::now();

          // Открываем файл лога для записи времени выполнения хода бота
          ofstream fout(project_path + "log.txt", ios_base::app);
          fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
          fout.close();
      }

    // Обрабатывает ход игрока
    // Параметр color: цвет игрока (false - белые, true - черные)
    // Возвращает Response - результат выполнения хода
    Response player_turn(const bool color)
    {
        // Подготавливаем список клеток с возможными ходами
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y); // Добавляем начальные позиции всех возможных ходов
        }
        board.highlight_cells(cells); // Подсвечиваем клетки с возможными ходами

        move_pos pos = { -1, -1, -1, -1 }; // Позиция для хода (x,y - откуда, x2,y2 - куда, xb,yb - бить)
        POS_T x = -1, y = -1; // Текущие координаты выбранной шашки

        // Цикл выбора и выполнения первого хода
        while (true)
        {
            // Получаем от игрока выбор клетки
            auto resp = hand.get_cell();

            // Если игрок выбрал не клетку (например, нажал кнопку меню)
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp); // Возвращаем действие (QUIT, BACK и т.д.)

            // Получаем координаты выбранной клетки
            pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

            // Проверяем корректность выбора
            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                // Если выбрана шашка, которой можно ходить
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                // Если выбрана клетка для завершения хода
                if (turn == move_pos{ x, y, cell.first, cell.second })
                {
                    pos = turn; // Запоминаем полный ход
                    break;
                }
            }

            // Если ход полностью выбран
            if (pos.x != -1)
                break;

            // Если выбор некорректен
            if (!is_correct)
            {
                // Сбрасываем выделение если была выбрана шашка
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells); // Восстанавливаем исходные возможные ходы
                }
                x = -1;
                y = -1;
                continue;
            }

            // Запоминаем выбранную шашку
            x = cell.first;
            y = cell.second;

            // Обновляем интерфейс
            board.clear_highlight();
            board.set_active(x, y); // Выделяем выбранную шашку

            // Подготавливаем список возможных целевых клеток для выбранной шашки
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2); // Добавляем возможные целевые позиции
                }
            }
            board.highlight_cells(cells2); // Подсвечиваем возможные ходы для выбранной шашки
        }

        // Очищаем выделения и выполняем ход
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1); // Перемещаем шашку (true если был бой)

        // Если не было боя - завершаем ход
        if (pos.xb == -1)
            return Response::OK;

        // Обработка серии взятий (несколько боёв за один ход)
        beat_series = 1; // Уже было одно взятие
        while (true)
        {
            // Ищем возможные продолжения боя для текущей шашки
            logic.find_turns(pos.x2, pos.y2);
            if (!logic.have_beats) // Если больше бить нельзя
                break;

            // Подготавливаем список клеток, куда можно бить
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells); // Подсвечиваем клетки для продолжения боя
            board.set_active(pos.x2, pos.y2); // Выделяем текущую шашку

            // Цикл выбора продолжения боя
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

                // Проверяем корректность выбора продолжения боя
                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn; // Запоминаем ход с взятием
                        break;
                    }
                }
                if (!is_correct)
                    continue;

                // Выполняем взятие
                board.clear_highlight();
                board.clear_active();
                beat_series += 1; // Увеличиваем счетчик серии взятий
                board.move_piece(pos, beat_series); // Выполняем ход с взятием
                break;
            }
        }

        return Response::OK; // Успешное завершение хода
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
