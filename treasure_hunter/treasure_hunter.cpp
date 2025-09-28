#include <iostream>
#include <vector>
#include <memory>
#include <random>

using namespace std;

// ===== Паттерн НАБЛЮДАТЕЛЬ (Observer) =====
enum class EventType {
    TreasureCollected,
    TrapTriggered
};

class Observer {
public:
    virtual ~Observer() = default;
    virtual void onNotify(EventType event) = 0;
};

class Subject {
private:
    vector<Observer*> observers;

public:
    void addObserver(Observer* observer) {
        observers.push_back(observer);
    }

    void notify(EventType event) {
        for (auto obs : observers) {
            obs->onNotify(event);
        }
    }
};

// ===== Паттерн СИНГЛТОН (Singleton) =====
class GameManager : public Observer {
private:
    static GameManager* instance;
    int score;
    bool gameOver;
    int level;
    int treasuresToWin;

    GameManager() : score(0), gameOver(false), level(1), treasuresToWin(3) {}

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
        level = 1;
        treasuresToWin = 3;
    }

    bool isGameOver() const { return gameOver; }
    int getScore() const { return score; }
    int getLevel() const { return level; }
    int getTreasuresToWin() const { return treasuresToWin; }

    void addScore(int points) {
        score += points;
        if (score >= treasuresToWin) {
            levelUp();
        }
    }

    void levelUp() {
        level++;
        treasuresToWin += 2;
        score = 0;
    }

    void endGame() {
        gameOver = true;
    }

    void onNotify(EventType event) override {
        switch (event) {
            case EventType::TreasureCollected:
                addScore(1);
                break;
            case EventType::TrapTriggered:
                endGame();
                break;
        }
    }
};

GameManager* GameManager::instance = nullptr;

// Базовый класс игровых объектов
class GameObject {
protected:
    int x, y;
    char symbol;
    bool active;

public:
    GameObject(int posX, int posY, char sym) : x(posX), y(posY), symbol(sym), active(true) {}
    virtual ~GameObject() = default;

    int getX() const { return x; }
    int getY() const { return y; }
    char getSymbol() const { return symbol; }
    bool isActive() const { return active; }
    void setActive(bool act) { active = act; }
};

// ===== Паттерн ФАБРИКА (Factory) =====
class GameObjectFactory {
private:
    random_device rd;
    mt19937 gen;

public:
    GameObjectFactory() : gen(rd()) {}

    enum class ObjectType {
        Treasure,
        Trap
    };

    unique_ptr<GameObject> createObject(ObjectType type, int x, int y) {
        switch (type) {
            case ObjectType::Treasure:
                return make_unique<GameObject>(x, y, 'T');
            case ObjectType::Trap:
                return make_unique<GameObject>(x, y, 'X');
            default:
                return nullptr;
        }
    }

    unique_ptr<GameObject> createRandomObject(int x, int y) {
        uniform_int_distribution<> dist(0, 4);
        if (dist(gen) == 0) {
            return createObject(ObjectType::Trap, x, y);
        } else {
            return createObject(ObjectType::Treasure, x, y);
        }
    }
};

// Класс игрока
class Player : public GameObject, public Subject {
private:
    int fieldWidth, fieldHeight;

public:
    Player(int width, int height) : GameObject(width/2, height/2, '@'), fieldWidth(width), fieldHeight(height) {}

    void move(int dx, int dy) {
        int newX = x + dx;
        int newY = y + dy;
        
        if (newX >= 0 && newX < fieldWidth && newY >= 0 && newY < fieldHeight) {
            x = newX;
            y = newY;
        }
    }
};

// Класс игрового поля
class GameField {
private:
    int width, height;
    vector<unique_ptr<GameObject>> objects;
    GameObjectFactory factory;

public:
    GameField(int w, int h) : width(w), height(h) {
        generateField();
    }

    void generateField() {
        objects.clear();
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (x != width/2 || y != height/2) {
                    auto obj = factory.createRandomObject(x, y);
                    if (obj) {
                        objects.push_back(move(obj));
                    }
                }
            }
        }
    }

    GameObject* getObjectAt(int x, int y) {
        for (auto& obj : objects) {
            if (obj->isActive() && obj->getX() == x && obj->getY() == y) {
                return obj.get();
            }
        }
        return nullptr;
    }

    void removeObject(GameObject* object) {
        for (auto& obj : objects) {
            if (obj.get() == object) {
                obj->setActive(false);
                break;
            }
        }
    }

    void addTrap() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> xDist(0, width-1);
        uniform_int_distribution<> yDist(0, height-1);
        
        int x, y;
        int attempts = 0;
        do {
            x = xDist(gen);
            y = yDist(gen);
            attempts++;
        } while (getObjectAt(x, y) != nullptr && attempts < 50);
        
        if (attempts < 50) {
            objects.push_back(factory.createObject(GameObjectFactory::ObjectType::Trap, x, y));
        }
    }

    void draw(int playerX, int playerY) const {
        // Верхняя граница
        cout << "+";
        for (int x = 0; x < width * 2 + 1; x++) cout << "-";
        cout << "+\n";
        
        // Поле
        for (int y = 0; y < height; y++) {
            cout << "| ";
            for (int x = 0; x < width; x++) {
                if (x == playerX && y == playerY) {
                    cout << "@ "; // Игрок
                } else {
                    bool found = false;
                    for (auto& obj : objects) {
                        if (obj->isActive() && obj->getX() == x && obj->getY() == y) {
                            cout << obj->getSymbol() << " ";
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        cout << ". ";
                    }
                }
            }
            cout << "|\n";
        }
        
        // Нижняя граница
        cout << "+";
        for (int x = 0; x < width * 2 + 1; x++) cout << "-";
        cout << "+\n";
    }
};

// Простая функция для получения ввода
char getInput() {
    char c;
    cin >> c;
    cin.ignore(); // очищаем буфер
    return c;
}

int main() {
    GameManager& game = GameManager::getInstance();
    game.init();

    const int FIELD_WIDTH = 10;
    const int FIELD_HEIGHT = 6;
    
    GameField field(FIELD_WIDTH, FIELD_HEIGHT);
    Player player(FIELD_WIDTH, FIELD_HEIGHT);
    player.addObserver(&game);

    cout << "=== ОХОТНИК ЗА СОКРОВИЩАМИ ===\n";
    cout << "Соберите сокровища (T), избегая ловушек (X)\n";
    cout << "Управление: W - вверх, A - влево, S - вниз, D - вправо, Q - выход\n";
    cout << "Нажмите Enter чтобы начать...";
    getInput();

    while (!game.isGameOver()) {
        // Отрисовка
        cout << "\n\n";
        field.draw(player.getX(), player.getY());
        
        cout << "Сокровища: " << game.getScore() << "/" << game.getTreasuresToWin();
        cout << " | Уровень: " << game.getLevel() << "\n";
        cout << "Легенда: @ - вы, T - сокровище, X - ловушка\n";
        
        if (game.isGameOver()) {
            cout << "💀 ИГРА ОКОНЧЕНА! Собрано сокровищ: " << game.getScore() << " 💀\n";
            break;
        }
        
        cout << "Ваш ход (W/A/S/D/Q): ";
        char key = getInput();
        
        switch (key) {
            case 'w': case 'W': player.move(0, -1); break;
            case 's': case 'S': player.move(0, 1); break;
            case 'a': case 'A': player.move(-1, 0); break;
            case 'd': case 'D': player.move(1, 0); break;
            case 'q': case 'Q': game.endGame(); continue;
            default: cout << "Неверная команда!\n"; continue;
        }

        GameObject* collidedObject = field.getObjectAt(player.getX(), player.getY());
        if (collidedObject) {
            if (collidedObject->getSymbol() == 'T') {
                player.notify(EventType::TreasureCollected);
                field.removeObject(collidedObject);
                field.addTrap();
                cout << "Найдено сокровище! Появилась новая ловушка.\n";
            } else if (collidedObject->getSymbol() == 'X') {
                player.notify(EventType::TrapTriggered);
                cout << "Вы наступили на ловушку!\n";
            }
        }
    }

    cout << "Спасибо за игру!\n";
    return 0;
}