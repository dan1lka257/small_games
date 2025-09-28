#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <thread>
#include <chrono>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>

// ===== Linux-специфичные функции для неблокирующего ввода =====
bool kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return true;
    }

    return false;
}

char getch() {
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return (buf);
}
// ===== Конец Linux-специфичных функций =====

// Предварительные объявления
class GameObject;
class Player;
class Enemy;

// ===== Паттерн НАБЛЮДАТЕЛЬ (Observer) =====
enum class EventType {
    EnemyHit,
    PlayerHit
};

class Observer {
public:
    virtual ~Observer() = default;
    virtual void onNotify(const GameObject& entity, EventType event) = 0;
};

class Subject {
private:
    std::vector<Observer*> observers;

public:
    void addObserver(Observer* observer) {
        observers.push_back(observer);
    }

    void notify(const GameObject& entity, EventType event) {
        for (auto obs : observers) {
            obs->onNotify(entity, event);
        }
    }
};
// ===== Конец паттерна НАБЛЮДАТЕЛЬ =====

// Базовый класс для всех игровых объектов
class GameObject {
protected:
    int x, y;
    char symbol;
    bool isActive;

public:
    GameObject(int startX, int startY, char s) : x(startX), y(startY), symbol(s), isActive(true) {}
    virtual ~GameObject() = default;

    int getX() const { return x; }
    int getY() const { return y; }
    char getSymbol() const { return symbol; }
    bool active() const { return isActive; }
    virtual void setActive(bool active) { isActive = active; } // Сделали виртуальным

    virtual void update() = 0;
    virtual void draw() const {
        if (isActive) {
            std::cout << "\033[" << y << ";" << x << "H" << symbol;
        }
    }
};

// ===== Паттерн СИНГЛТОН (Singleton) =====
class GameManager : public Observer {
private:
    static GameManager* instance;
    int score;
    bool gameOver;
    int screenWidth, screenHeight;

    GameManager() : score(0), gameOver(false), screenWidth(80), screenHeight(24) {}

public:
    GameManager(const GameManager&) = delete;
    GameManager& operator=(const GameManager&) = delete;

    static GameManager& getInstance() {
        if (instance == nullptr) {
            instance = new GameManager();
        }
        return *instance;
    }

    void init() {
        score = 0;
        gameOver = false;
        std::cout << "\033[2J";
        std::cout << "\033[?25l";
    }

    ~GameManager() {
        std::cout << "\033[?25h";
    }

    bool isGameOver() const { return gameOver; }
    int getScore() const { return score; }
    int getScreenWidth() const { return screenWidth; }
    int getScreenHeight() const { return screenHeight; }

    void addScore(int points) {
        score += points;
    }

    void endGame() {
        gameOver = true;
    }

    void onNotify(const GameObject& entity, EventType event) override {
        switch (event) {
            case EventType::EnemyHit:
                addScore(10);
                break;
            case EventType::PlayerHit:
                endGame();
                break;
        }
    }

    void drawUI() {
        std::cout << "\033[1;1H";
        std::cout << "Score: " << score << " | Press 'q' to quit, 'f' to fire";
        std::cout << std::flush;
        if (gameOver) {
            std::cout << "\033[" << screenHeight / 2 << ";" << (screenWidth - 10) / 2 << "H";
            std::cout << "GAME OVER!";
            std::cout << "\033[" << screenHeight / 2 + 1 << ";" << (screenWidth - 15) / 2 << "H";
            std::cout << "Final Score: " << score;
        }
    }
};

GameManager* GameManager::instance = nullptr;
// ===== Конец паттерна СИНГЛТОН =====

class Enemy : public GameObject, public Subject {
protected:
    int speed;

public:
    Enemy(int x, int y, char s, int spd) : GameObject(x, y, s), speed(spd) {}

    void update() override {
        if (!isActive) return;
        y += speed;
        if (y > GameManager::getInstance().getScreenHeight()) {
            isActive = false;
        }
    }
};

class FastEnemy : public Enemy {
public:
    FastEnemy(int x) : Enemy(x, 3, 'F', 2) {}
};

class ToughEnemy : public Enemy {
private:
    int health = 2;

public:
    ToughEnemy(int x) : Enemy(x, 3, 'T', 1) {}

    // Убрали override, так как в базовом классе метод не виртуальный
    void setActive(bool active) {
        if (active) {
            isActive = true;
        } else {
            health--;
            if (health <= 0) {
                isActive = false;
                notify(*this, EventType::EnemyHit);
            }
        }
    }
};

// ===== Паттерн ФАБРИКА (Factory) =====
class EnemyFactory {
private:
    std::random_device rd;
    std::mt19937 gen;

public:
    EnemyFactory() : gen(rd()) {}

    std::unique_ptr<Enemy> createRandomEnemy() {
        std::uniform_int_distribution<> typeDist(0, 1);
        std::uniform_int_distribution<> posDist(1, GameManager::getInstance().getScreenWidth() - 2);

        int x = posDist(gen);
        Enemy* newEnemy = nullptr;

        switch (typeDist(gen)) {
            case 0:
                newEnemy = new FastEnemy(x);
                break;
            case 1:
                newEnemy = new ToughEnemy(x);
                break;
        }

        newEnemy->addObserver(&GameManager::getInstance());
        return std::unique_ptr<Enemy>(newEnemy);
    }
};
// ===== Конец паттерна ФАБРИКА =====

class Bullet : public GameObject {
public:
    Bullet(int startX, int startY) : GameObject(startX, startY, '|') {}

    void update() override {
        if (!isActive) return;
        y--;
        if (y <= 1) {
            isActive = false;
        }
    }
};

class Player : public GameObject, public Subject {
private:
    std::vector<std::unique_ptr<Bullet>> bullets; // Используем умные указатели
    int fireCooldown;

public:
    Player() : GameObject(40, 20, 'A'), fireCooldown(0) {}

    void update() override {
        if (fireCooldown > 0) {
            fireCooldown--;
        }
        
        for (auto& bullet : bullets) {
            bullet->update();
        }
        
        bullets.erase(
            std::remove_if(bullets.begin(), bullets.end(),
                           [](const std::unique_ptr<Bullet>& b) { return !b->active(); }),
            bullets.end()
        );
    }

    void draw() const override {
        GameObject::draw();
        for (const auto& bullet : bullets) {
            bullet->draw();
        }
    }

    void moveLeft() {
        if (x > 1) x--;
    }

    void moveRight() {
        if (x < GameManager::getInstance().getScreenWidth() - 2) x++;
    }

    void fire() {
        if (fireCooldown == 0) {
            bullets.push_back(std::make_unique<Bullet>(x, y - 1));
            fireCooldown = 5;
        }
    }

    const std::vector<std::unique_ptr<Bullet>>& getBullets() const { return bullets; }

    bool checkCollision(const GameObject& other) const {
        return (active() && other.active() && x == other.getX() && y == other.getY());
    }
};

int main() {
    GameManager& game = GameManager::getInstance();
    game.init();

    EnemyFactory factory;
    Player player;
    player.addObserver(&game);

    std::vector<std::unique_ptr<Enemy>> enemies;

    auto lastEnemyTime = std::chrono::steady_clock::now();

    while (!game.isGameOver()) {
        std::cout << "\033[2J";

        // Генерация врагов
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEnemyTime).count() > 1500) {
            enemies.push_back(factory.createRandomEnemy());
            lastEnemyTime = now;
        }

        // Обработка ввода
        if (kbhit()) {
            char key = getch();
            switch (key) {
                case 'a': player.moveLeft(); break;
                case 'd': player.moveRight(); break;
                case 'f': player.fire(); break;
                case 'q': game.endGame(); break;
            }
        }

        // Обновление состояния
        player.update();
        for (auto& enemy : enemies) {
            enemy->update();
        }

        // Проверка столкновений пуль с врагами
        for (auto& enemy : enemies) {
            for (const auto& bullet : player.getBullets()) {
                if (enemy->active() && bullet->active() && 
                    enemy->getX() == bullet->getX() && enemy->getY() == bullet->getY()) {
                    bullet->setActive(false);
                    enemy->setActive(false);
                }
            }
            // Проверка столкновения врага с игроком
            if (player.checkCollision(*enemy)) {
                player.notify(*enemy, EventType::PlayerHit);
            }
        }

        // Удаление неактивных врагов
        enemies.erase(
            std::remove_if(enemies.begin(), enemies.end(),
                           [](const std::unique_ptr<Enemy>& e) { return !e->active(); }),
            enemies.end()
        );

        // Отрисовка
        player.draw();
        for (auto& enemy : enemies) {
            enemy->draw();
        }
        game.drawUI();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "\033[2J";
    game.drawUI();
    std::cout << std::endl;
    
    return 0;
}