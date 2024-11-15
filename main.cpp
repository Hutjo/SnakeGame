#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <deque>
#include <vector>
#include <string>
#include <sqlite3.h>
#include <ctime>
#include <algorithm>

// Game constants
namespace GameConstants {
    constexpr int CELL_SIZE = 30;
    constexpr int CELL_COUNT = 25;
    constexpr int OFFSET = 60;
    constexpr float GAME_SPEED = 0.2f;
    
    // Colors
    const Color BACKGROUND_COLOR = {0, 0, 140, 255};
    const Color SNAKE_COLOR = {0, 0, 0, 255};
    const Color TEXT_COLOR = {255, 255, 255, 255};
}

// Utility functions
namespace Utils {
    bool isPointInDeque(const Vector2& point, const std::deque<Vector2>& deque) {
        return std::any_of(deque.begin(), deque.end(),
            [&point](const Vector2& element) { return Vector2Equals(element, point); });
    }

    Vector2 generateRandomPosition(int min, int max) {
        return Vector2{
            static_cast<float>(GetRandomValue(min, max)),
            static_cast<float>(GetRandomValue(min, max))
        };
    }

    std::string getCurrentDateTime(const char* format) {
        std::time_t currentTime = std::time(nullptr);
        struct tm* localTime = std::localtime(&currentTime);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), format, localTime);
        return std::string(buffer);
    }
}

class HighscoreManager {
public:
    struct ScoreEntry {
        std::string date;
        std::string time;
        int score;
    };

    HighscoreManager() {
        initializeDatabase();
    }

    ~HighscoreManager() {
        if (db) sqlite3_close(db);
    }

    void saveHighscore(int score) {
        const std::string date = Utils::getCurrentDateTime("%Y-%m-%d");
        const std::string time = Utils::getCurrentDateTime("%H:%M:%S");
        
        const char* sql = "INSERT INTO highscores (date, time, score) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, date.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, time.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, score);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    std::vector<ScoreEntry> getTopScores(int limit = 10) const {
        std::vector<ScoreEntry> scores;
        const char* sql = "SELECT date, time, score FROM highscores ORDER BY score DESC LIMIT ?";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, limit);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                scores.push_back({
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                    reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                    sqlite3_column_int(stmt, 2)
                });
            }
            sqlite3_finalize(stmt);
        }
        return scores;
    }

private:
    sqlite3* db = nullptr;

    void initializeDatabase() {
        if (sqlite3_open("highscores.db", &db) == SQLITE_OK) {
            const char* sql = "CREATE TABLE IF NOT EXISTS highscores ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                            "date TEXT, time TEXT, score INTEGER)";
            sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
        }
    }
};

class Snake {
public:
    Snake() : body_{{6, 9}, {5, 9}, {4, 9}}, direction_{1, 0} {}

    bool willCollide(const Vector2& newPos) const {
        // Check if the new position collides with body
        auto bodyCheck = body_;
        if (!shouldGrow_) {
            bodyCheck.pop_back(); // Remove tail if not growing
        }
        return Utils::isPointInDeque(newPos, bodyCheck);
    }

    bool update() {
        if (!shouldUpdate_) return true;

        // Calculate new head position
        Vector2 newHead = Vector2Add(body_.front(), direction_);
        
        // Store for external collision checking
        nextPosition_ = newHead;
        
        // Move the snake
        body_.push_front(newHead);
        if (!shouldGrow_) {
            body_.pop_back();
        }
        shouldGrow_ = false;
        shouldUpdate_ = true;

        return true;
    }

    void draw() const {
        for (const auto& segment : body_) {
            Rectangle segmentRect{
                GameConstants::OFFSET + segment.x * GameConstants::CELL_SIZE,
                GameConstants::OFFSET + segment.y * GameConstants::CELL_SIZE,
                static_cast<float>(GameConstants::CELL_SIZE),
                static_cast<float>(GameConstants::CELL_SIZE)
            };
            DrawRectangleRounded(segmentRect, 0.5, 6, GameConstants::SNAKE_COLOR);
        }
    }

    void setDirection(Vector2 newDir) {
        // Prevent diagonal movement and opposite direction
        if (direction_.x != -newDir.x && direction_.y != -newDir.y &&
            !(newDir.x != 0 && newDir.y != 0)) {  // Prevent diagonal movement
            direction_ = newDir;
        }
        shouldUpdate_ = true;
    }

    void grow() { shouldGrow_ = true; }
    
    void reset() {
        body_ = {{6, 9}, {5, 9}, {4, 9}};
        direction_ = {1, 0};
        shouldGrow_ = false;
        shouldUpdate_ = true;
    }

    const std::deque<Vector2>& getBody() const { return body_; }
    Vector2 getHead() const { return body_.front(); }
    Vector2 getDirection() const { return direction_; }
    Vector2 getNextPosition() const { return nextPosition_; }

private:
    std::deque<Vector2> body_;
    Vector2 direction_;
    Vector2 nextPosition_;
    bool shouldGrow_ = false;
    bool shouldUpdate_ = true;
};

class Collectible {
public:
    explicit Collectible(const std::deque<Vector2>& snakeBody) {
        loadTexture();
        resetPosition(snakeBody);
    }

    ~Collectible() {
        UnloadTexture(texture_);
    }

    void draw() const {
        DrawTexture(texture_,
            GameConstants::OFFSET + position_.x * GameConstants::CELL_SIZE - textureWidth_ / 2,
            GameConstants::OFFSET + position_.y * GameConstants::CELL_SIZE - textureHeight_ / 2,
            GameConstants::BACKGROUND_COLOR);
    }

    void resetPosition(const std::deque<Vector2>& snakeBody) {
        do {
            position_ = Utils::generateRandomPosition(1, GameConstants::CELL_COUNT - 2);
        } while (Utils::isPointInDeque(position_, snakeBody));
    }

    Vector2 getPosition() const { return position_; }

private:
    Vector2 position_;
    Texture2D texture_;
    float textureWidth_;
    float textureHeight_;

    void loadTexture() {
        Image image = LoadImage("Images/fish.png");
        ImageResize(&image, GameConstants::CELL_SIZE * 2, GameConstants::CELL_SIZE * 2);
        texture_ = LoadTextureFromImage(image);
        textureWidth_ = static_cast<float>(GameConstants::CELL_SIZE * 2);
        textureHeight_ = static_cast<float>(GameConstants::CELL_SIZE * 2);
        UnloadImage(image);
    }
};

class Game {
public:
    Game() : snake_(), collectible_(snake_.getBody()) {}

    void update() {
        if (!isRunning_ || isPaused_) return;

        // Calculate and check next position before any movement
        if (willCollide()) {
            gameOver();
            return;
        }

        // Only update if no collision was detected
        snake_.update();
        checkCollectible();
    }

    void draw() {
        if (showHighScores_) {
            drawHighScores();
            return;
        }

        collectible_.draw();
        snake_.draw();
        
        DrawText(TextFormat("Score: %i", score_), 
                GameConstants::OFFSET - 5, 20, 40, 
                GameConstants::SNAKE_COLOR);
        
        if (isPaused_) {
            DrawText("PAUSE", 
                    GameConstants::OFFSET + GameConstants::CELL_SIZE * GameConstants::CELL_COUNT / 2 - 40,
                    GameConstants::OFFSET + GameConstants::CELL_SIZE * GameConstants::CELL_COUNT / 2 - 10,
                    30, GameConstants::TEXT_COLOR);
        }
        
        if (!isRunning_ && !isPaused_) {
            DrawText("GAME OVER - Press any arrow key to restart", 
                    GameConstants::OFFSET, 
                    GameConstants::OFFSET + GameConstants::CELL_SIZE * GameConstants::CELL_COUNT / 2, 
                    20, GameConstants::TEXT_COLOR);
        }
    }

    void handleInput() {
        if (IsKeyPressed(KEY_P)) togglePause();
        if (IsKeyPressed(KEY_H)) toggleHighScores();

        if (!isPaused_) {
            // Only handle one key press per frame
            if (IsKeyPressed(KEY_UP) && snake_.getDirection().y != 1) {
                if (!isRunning_) resetGame();
                snake_.setDirection({0, -1});
                isRunning_ = true;
            }
            else if (IsKeyPressed(KEY_DOWN) && snake_.getDirection().y != -1) {
                if (!isRunning_) resetGame();
                snake_.setDirection({0, 1});
                isRunning_ = true;
            }
            else if (IsKeyPressed(KEY_RIGHT) && snake_.getDirection().x != -1) {
                if (!isRunning_) resetGame();
                snake_.setDirection({1, 0});
                isRunning_ = true;
            }
            else if (IsKeyPressed(KEY_LEFT) && snake_.getDirection().x != 1) {
                if (!isRunning_) resetGame();
                snake_.setDirection({-1, 0});
                isRunning_ = true;
            }
        }
    }

private:
    Snake snake_;
    Collectible collectible_;
    HighscoreManager highscoreManager_;
    bool isRunning_ = true;
    bool isPaused_ = false;
    bool showHighScores_ = false;
    int score_ = 0;

    void resetGame() {
        snake_.reset();
        collectible_.resetPosition(snake_.getBody());
        score_ = 0;
        isRunning_ = true;
        isPaused_ = false;
    }

    bool willCollide() {
        Vector2 nextPos = Vector2Add(snake_.getHead(), snake_.getDirection());
            
        // Check wall collision first
        if (nextPos.x < 0 || nextPos.x >= GameConstants::CELL_COUNT ||
            nextPos.y < 0 || nextPos.y >= GameConstants::CELL_COUNT) {
            return true;
        }

        // Check self collision
        auto body = snake_.getBody();
        body.pop_front();  // Remove current head for checking
        return Utils::isPointInDeque(nextPos, body);
    }

    void checkCollectible() {
        if (Vector2Equals(snake_.getHead(), collectible_.getPosition())) {
            collectible_.resetPosition(snake_.getBody());
            snake_.grow();
            score_++;
        }
    }

    void gameOver() {
        highscoreManager_.saveHighscore(score_);
        isRunning_ = false;
    }

    void togglePause() { 
        isPaused_ = !isPaused_; 
    }
    
    void toggleHighScores() {
        showHighScores_ = !showHighScores_;
        isPaused_ = showHighScores_;
    }

    void drawHighScores() {
        ClearBackground(GameConstants::BACKGROUND_COLOR);
        DrawText("HIGH SCORES", GameConstants::OFFSET - 10, 7, 40, GameConstants::SNAKE_COLOR);
        
        const auto scores = highscoreManager_.getTopScores(10);
        int yPos = 120;
        
        for (size_t i = 0; i < scores.size(); i++) {
            const auto& entry = scores[i];
            std::string scoreText = TextFormat("#%d: %d pts - %s %s", 
                                             i + 1, entry.score, 
                                             entry.date.c_str(), 
                                             entry.time.c_str());
            DrawText(scoreText.c_str(), GameConstants::OFFSET, yPos, 20, GameConstants::SNAKE_COLOR);
            yPos += 30;
        }
        
        DrawText("Press H to return to game", GameConstants::OFFSET, yPos + 40, 20, GameConstants::SNAKE_COLOR);
    }
};

int main() {
    InitWindow(2 * GameConstants::OFFSET + GameConstants::CELL_SIZE * GameConstants::CELL_COUNT,
               2 * GameConstants::OFFSET + GameConstants::CELL_SIZE * GameConstants::CELL_COUNT,
               "Snake Game");
    SetTargetFPS(60);

    Game game;
    double lastUpdateTime = 0;

    while (!WindowShouldClose()) {
        double currentTime = GetTime();
        if (currentTime - lastUpdateTime >= GameConstants::GAME_SPEED) {
            game.update();
            lastUpdateTime = currentTime;
        }

        game.handleInput();

        BeginDrawing();
        ClearBackground(GameConstants::BACKGROUND_COLOR);
        
        // Draw game border
        DrawRectangleLinesEx(
            Rectangle{
                static_cast<float>(GameConstants::OFFSET - 5),
                static_cast<float>(GameConstants::OFFSET - 5),
                static_cast<float>(GameConstants::CELL_SIZE * GameConstants::CELL_COUNT + 10),
                static_cast<float>(GameConstants::CELL_SIZE * GameConstants::CELL_COUNT + 10)
            },
            5, GameConstants::SNAKE_COLOR
        );

        game.draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}