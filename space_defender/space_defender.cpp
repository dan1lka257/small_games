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
    return buf;
}

class GameObject;
class Player;
class Enemy;

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

class GameObject {
protected:
    int x, y;
    char symbol;
    bool is_active;

public:
    GameObject(int start_x, int start_y, char s) : x(start_x), y(start_y), symbol(s), is_active(true) {}
    virtual ~GameObject() = default;

    int getX() const { return x; }
    int getY() const { return y; }
    char getSymbol() const { return symbol; }
    bool active() const { return is_active; }
    virtual void setActive(bool active) { is_active = active; }

    virtual void update() = 0;
    virtual void draw() const {
        if (is_active) {
            std::cout << "\033[" << y << ";" << x << "H" << symbol;
        }
    }
};

class GameManager : public Observer {
private:
    static GameManager* instance;
    int score;
    bool game_over;
    int screen_width, screen_height;

    GameManager() : score(0), game_over(false), screen_width(64), screen_height(60) {}

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
        game_over = false;
        std::cout << "\033[2J";
        std::cout << "\033[?25l";
    }

    ~GameManager() {
        std::cout << "\033[?25h";
    }

    bool isGameOver() const { return game_over; }
    int getScore() const { return score; }
    int getScreenWidth() const { return screen_width; }
    int getScreenHeight() const { return screen_height; }

    void addScore(int points) {
        score += points;
    }

    void endGame() {
        game_over = true;
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
        if (game_over) {
            std::cout << "\033[" << screen_height / 2 << ";" << (screen_width - 10) / 2 << "H";
            std::cout << "GAME OVER!";
            std::cout << "\033[" << screen_height / 2 + 1 << ";" << (screen_width - 15) / 2 << "H";
            std::cout << "Final Score: " << score;
        }
    }
};

GameManager* GameManager::instance = nullptr;

class Enemy : public GameObject, public Subject {
protected:
    int speed;

public:
    Enemy(int x, int y, char s, int spd) : GameObject(x, y, s), speed(spd) {}

    void update() override {
        if (!is_active) return;
        y += speed;
        if (y > GameManager::getInstance().getScreenHeight()) {
            is_active = false;
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

    void setActive(bool active) {
        if (active) {
            is_active = true;
        } else {
            health--;
            if (health <= 0) {
                is_active = false;
                notify(*this, EventType::EnemyHit);
            }
        }
    }
};

class EnemyFactory {
private:
    std::random_device rd;
    std::mt19937 gen;

public:
    EnemyFactory() : gen(rd()) {}

    std::unique_ptr<Enemy> createRandomEnemy() {
        std::uniform_int_distribution<> type_dist(0, 1);
        std::uniform_int_distribution<> pos_dist(1, GameManager::getInstance().getScreenWidth() - 2);

        int x = pos_dist(gen);
        std::unique_ptr<Enemy> new_enemy;

        switch (type_dist(gen)) {
            case 0:
                new_enemy = std::make_unique<FastEnemy>(x);
                break;
            case 1:
                new_enemy = std::make_unique<ToughEnemy>(x);
                break;
        }

        new_enemy->addObserver(&GameManager::getInstance());
        return new_enemy;
    }
};

class Bullet : public GameObject {
public:
    Bullet(int start_x, int start_y) : GameObject(start_x, start_y, '|') {}

    void update() override {
        if (!is_active) return;
        y--;
        if (y <= 1) {
            is_active = false;
        }
    }
};

class Player : public GameObject, public Subject {
private:
    std::vector<std::unique_ptr<Bullet>> bullets;
    int fire_cooldown;

public:
    Player() : GameObject(32, 50, 'A'), fire_cooldown(0) {}

    void update() override {
        if (fire_cooldown > 0) {
            fire_cooldown--;
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
        if (fire_cooldown == 0) {
            bullets.push_back(std::make_unique<Bullet>(x, y - 1));
            fire_cooldown = 5;
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

    auto last_enemy_time = std::chrono::steady_clock::now();

    while (!game.isGameOver()) {
        std::cout << "\033[2J";

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_enemy_time).count() > 1500) {
            enemies.push_back(factory.createRandomEnemy());
            last_enemy_time = now;
        }

        if (kbhit()) {
            char key = getch();
            switch (key) {
                case 'a': player.moveLeft(); break;
                case 'd': player.moveRight(); break;
                case 'f': player.fire(); break;
                case 'q': game.endGame(); break;
            }
        }

        player.update();
        for (auto& enemy : enemies) {
            enemy->update();
        }

        for (auto& enemy : enemies) {
            for (const auto& bullet : player.getBullets()) {
                if (enemy->active() && bullet->active() && 
                    enemy->getX() == bullet->getX() && enemy->getY() == bullet->getY()) {
                    bullet->setActive(false);
                    enemy->setActive(false);
                }
            }
            if (player.checkCollision(*enemy)) {
                player.notify(*enemy, EventType::PlayerHit);
            }
        }

        enemies.erase(
            std::remove_if(enemies.begin(), enemies.end(),
                           [](const std::unique_ptr<Enemy>& e) { return !e->active(); }),
            enemies.end()
        );

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