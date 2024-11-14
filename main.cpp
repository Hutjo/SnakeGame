#include <iostream>
#include <raylib.h>
#include <raymath.h>
#include <deque>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>

#include <sqlite3.h>
#include <ctime>
using namespace std;


int cellSize = 30;
int cellCount = 25;
int offset = 60;


Color blue = {0, 0, 140, 255};
Color green = {173, 204, 96, 255};
Color white = {255, 255, 255, 255};
Color black = {0, 0, 0, 255};


double lastUpdate = 0;

bool ElementInDeque(Vector2 element, deque<Vector2>deque)
{

    for(unsigned int i = 0; i < deque.size(); i++)
    {
        if(Vector2Equals(deque[i], element))
        {
            return true;
        }
    }
    return false;

}

bool eventTr(double interval)
{
    double currentTime = GetTime();
    if(currentTime - lastUpdate >= interval)
    {
        lastUpdate = currentTime;
        return true;
    }
    return false;
}

class HighscoreManager {
public:
    struct ScoreEntry {
        std::string date;
        std::string time;
        int score;
    };

    HighscoreManager() {
        // Open the the db
        int rc = sqlite3_open("highscores.db", &db);
        if (rc != SQLITE_OK) {
            std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
            return;
        }

        // Create highscore table if it doesn't exist
        const char* sql = "CREATE TABLE IF NOT EXISTS highscores ("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                         "date TEXT, "
                         "time TEXT, "
                         "score INTEGER)";
        
        char* errMsg = nullptr;
        rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }

    ~HighscoreManager() {
        if (db) {
            sqlite3_close(db);
        }
    }

    void SaveHighscore(int score) {
        // Get the date & time
        std::time_t currentTime = std::time(nullptr);
        struct tm* localTime = std::localtime(&currentTime);
        char dateBuffer[11];
        char timeBuffer[9];
        std::strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", localTime);
        std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", localTime);

        // Insert the highscore into the db
        const char* sql = "INSERT INTO highscores (date, time, score) VALUES (?, ?, ?)";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, dateBuffer, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, timeBuffer, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, score);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        } else {
            std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
        }
    }

    int GetHighscore() {
        // Retrieve the highest score from the db
        const char* sql = "SELECT MAX(score) FROM highscores";
        sqlite3_stmt* stmt;
        int highscore = 0;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                highscore = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        } else {
            std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
        }
        
        return highscore;
    }

    std::vector<ScoreEntry> GetTopScores(int limit = 10) {
        std::vector<ScoreEntry> scores;
        const char* sql = "SELECT date, time, score FROM highscores ORDER BY score DESC LIMIT ?";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, limit);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                ScoreEntry entry;
                entry.date = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
                entry.time = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
                entry.score = sqlite3_column_int(stmt, 2);
                scores.push_back(entry);
            }
            
            sqlite3_finalize(stmt);
        }
        
        return scores;
    }

    void ClearScores() {
        char* errMsg = nullptr;
        const char* sql = "DELETE FROM highscores";
        
        int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    }

private:
    sqlite3* db;
};

class Snake
{
public:
    deque<Vector2> body = {Vector2{6,9}, Vector2{5,9}, Vector2{4,9}};
    Vector2 direction = {1, 0};
    bool addSeg = false;


    void Draw()
    {
        for(unsigned int i = 0; i < body.size(); i++)
        {
            float x = body[i].x;
            float y = body[i].y;
            Rectangle seg = Rectangle{offset + x*cellSize, offset + y*cellSize, float(cellSize), float(cellSize)};
            DrawRectangleRounded(seg, 0.5, 6, BLACK);
            
        }
    }
    void Update()
    {
        body.push_front(Vector2Add(body[0], direction));
        if(addSeg == true)
        {
            addSeg = false;
        }
        else
        {
            body.pop_back();
        }
        
    }
    void Reset()
    {
        body = {Vector2{6,9}, Vector2{5,9}, Vector2{4,9}};
        direction = {1, 0};
    }


};
class Collectibles
{

public:
    Vector2 position;
    Texture2D texture;
    float textureWidth, textureHeight;

      Collectibles(std::deque<Vector2> snakeBody)
    {
        Image image = LoadImage("Images/fish.png");
        // Scale the image to 2 times the cell size
        ImageResize(&image, cellSize * 2, cellSize * 2);
        texture = LoadTextureFromImage(image);
        textureWidth = static_cast<float>(cellSize * 2);
        textureHeight = static_cast<float>(cellSize * 2);
        UnloadImage(image);
        position = GenRandPos(snakeBody);
    }

    ~Collectibles()
    {
        UnloadTexture(texture);
    }

    void Draw()
    {
        DrawTexture(texture,offset + position.x * cellSize - textureWidth / 2, offset +position.y * cellSize - textureHeight / 2, blue);
    }

    Vector2 GenRandCell()
    {
        float x = GetRandomValue(1, cellCount -2 );
        float y = GetRandomValue(1, cellCount -2 );
        return Vector2{x, y};
    }

    Vector2 GenRandPos(deque<Vector2> snakeBody)
    {
        Vector2 position = GenRandCell();
        while(ElementInDeque(position, snakeBody))
        {
            position = GenRandCell();
        }
        return position;
    }

};
class Game
{
    public:
    Snake snake = Snake();
    Collectibles collectibles = Collectibles(snake.body);
    HighscoreManager highscoreManager = HighscoreManager();
    bool running = true;
    bool paused = false;
    bool showHighScores = false;
    
    
    int score = 0;

    void Draw() {
        if (showHighScores) {
            DrawHighScores();
        } else {
            collectibles.Draw();
            snake.Draw();
            
            // Draw current score 
            DrawText(TextFormat("Score: %i", score), offset - 5, 20, 40, BLACK);
            //DrawText(TextFormat("High Score: %i", highscoreManager.GetHighscore()), offset - 5, 70, 40, BLACK);
            
            if(paused) {
                DrawText("PAUSE", offset + cellSize * cellCount / 2 - 40, 
                        offset + cellSize * cellCount / 2 - 10, 30, white);
            }
        }
    }

    void DrawHighScores() {
        ClearBackground(blue);
        DrawText("HIGH SCORES", offset-5, 70, 40, BLACK);
        
        auto scores = highscoreManager.GetTopScores(10);
        int yPos = 120;
        
        for (size_t i = 0; i < scores.size(); i++) {
            const auto& entry = scores[i];
            std::string scoreText = TextFormat("#%d: %d pts - %s %s", 
                                             i + 1, entry.score, 
                                             entry.date.c_str(), 
                                             entry.time.c_str());
            DrawText(scoreText.c_str(), offset, yPos, 20, BLACK);
            yPos += 30;
        }
        
        DrawText("Press H to return to game", offset, yPos + 40, 20, BLACK);
    }


    void Update()
    {
        if(running && !paused)
        {
            snake.Update();
            CheckColCollectibles();
            CheckColEdges();
            CheckColTail();
        }
    }


    void CheckColCollectibles()
    {
        if(Vector2Equals(snake.body[0], collectibles.position))
        {
            collectibles.position = collectibles.GenRandPos(snake.body);
            snake.addSeg = true;
            score ++;
        }
    }  
    void CheckColEdges()
    {
        if(snake.body[0].x == cellCount || snake.body[0].x == -1)
        {
            GameOver();
        }
        if(snake.body[0].y == cellCount || snake.body[0].y == -1)
        {
            GameOver();
        }
    }

    void CheckColTail()
    {
        deque<Vector2> onlyBody = snake.body;
        onlyBody.pop_front();
        if(ElementInDeque(snake.body[0], onlyBody))
        {
            GameOver();
        }
    }

    void GameOver()
    {
        snake.Reset();
        collectibles.position = collectibles.GenRandPos(snake.body);
        highscoreManager.SaveHighscore(score);
        running = false;
        paused = false;
        score = 0;
    }
};
int main () 
{
    cout << "Game Starting " << endl;
    InitWindow(2 * offset + cellSize * cellCount, 2 * offset + cellSize * cellCount, "Snake");
    SetTargetFPS(60);

    Game game = Game();

    while(WindowShouldClose() == false)
    {
        BeginDrawing();

        if(eventTr(0.2))
        {
            if (!game.paused)
            {
                game.Update();
            }
        }
        if(IsKeyPressed(KEY_P))
        {
            game.paused = !game.paused;
        }
        if (IsKeyPressed(KEY_H)) 
        {
            game.showHighScores = !game.showHighScores;
            game.paused = game.showHighScores;  // Pause game while viewing scores
        }

        if(IsKeyPressed(KEY_UP) && game.snake.direction.y !=1)
        {
            game.snake.direction={0, -1};
            game.running = true;
            
        }
        if(IsKeyPressed(KEY_DOWN) && game.snake.direction.y !=-1)
        {
            game.snake.direction={0, 1};
            game.running = true;
            
        }
        if(IsKeyPressed(KEY_RIGHT)&& game.snake.direction.x !=-1)
        {
            game.snake.direction={1, 0};
            game.running = true;
            
        }
        if(IsKeyPressed(KEY_LEFT)&& game.snake.direction.x !=1)
        {
            game.snake.direction={-1, 0};
            game.running = true;
            
        }



        ClearBackground(blue);
        DrawRectangleLinesEx(Rectangle{(float)offset - 5, (float)offset - 5, (float)cellSize * cellCount + 10, (float)cellSize * cellCount + 10}, 5, BLACK);
        //DrawText(TextFormat("%i", game.score), offset -5, 20, 40, BLACK);
        game.Draw();

        EndDrawing();
    }

    CloseWindow();
    return 0;

}