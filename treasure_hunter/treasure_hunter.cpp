#include <iostream>
#include <vector>
#include <memory>
#include <random>

using namespace std;

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

class GameManager : public Observer {
private:
    static GameManager* instance;
    int score;
    bool game_over;
    int level;
    int treasures_to_win;

    GameManager() : score(0), game_over(false), level(1), treasures_to_win(3) {}

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
        level = 1;
        treasures_to_win = 3;
    }

    bool isGameOver() const { return game_over; }
    int getScore() const { return score; }
    int getLevel() const { return level; }
    int getTreasuresToWin() const { return treasures_to_win; }

    void addScore(int points) {
        score += points;
        if (score >= treasures_to_win) {
            levelUp();
        }
    }

    void levelUp() {
        level++;
        treasures_to_win += 2;
        score = 0;
    }

    void endGame() {
        game_over = true;
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

class GameObject {
protected:
    int x;
    int y;
    char symbol;
    bool active;

public:
    GameObject(int pos_x, int pos_y, char sym) : x(pos_x), y(pos_y), symbol(sym), active(true) {}
    virtual ~GameObject() = default;

    int getX() const { return x; }
    int getY() const { return y; }
    char getSymbol() const { return symbol; }
    bool isActive() const { return active; }
    void setActive(bool act) { active = act; }
};

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

class Player : public GameObject, public Subject {
private:
    int field_width;
    int field_height;

public:
    Player(int width, int height) : GameObject(width/2, height/2, '@'), field_width(width), field_height(height) {}

    void move(int dx, int dy) {
        int new_x = x + dx;
        int new_y = y + dy;
        
        if (new_x >= 0 && new_x < field_width && new_y >= 0 && new_y < field_height) {
            x = new_x;
            y = new_y;
        }
    }
};

class GameField {
private:
    int width;
    int height;
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
        uniform_int_distribution<> x_dist(0, width-1);
        uniform_int_distribution<> y_dist(0, height-1);
        
        int x;
        int y;
        int attempts = 0;
        do {
            x = x_dist(gen);
            y = y_dist(gen);
            attempts++;
        } while (getObjectAt(x, y) != nullptr && attempts < 50);
        
        if (attempts < 50) {
            objects.push_back(factory.createObject(GameObjectFactory::ObjectType::Trap, x, y));
        }
    }

    void draw(int player_x, int player_y) const {
        cout << "+";
        for (int x = 0; x < width * 2 + 1; x++) cout << "-";
        cout << "+\n";
        
        for (int y = 0; y < height; y++) {
            cout << "| ";
            for (int x = 0; x < width; x++) {
                if (x == player_x && y == player_y) {
                    cout << "@ ";
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
        
        cout << "+";
        for (int x = 0; x < width * 2 + 1; x++) cout << "-";
        cout << "+\n";
    }
};

char getInput() {
    char c;
    cin >> c;
    cin.ignore();
    return c;
}

int main() {
    GameManager& game = GameManager::getInstance();
    game.init();

    const int field_width = 10;
    const int field_height = 6;
    
    GameField field(field_width, field_height);
    Player player(field_width, field_height);
    player.addObserver(&game);

    cout << "=== ОХОТНИК ЗА СОКРОВИЩАМИ ===\n";
    cout << "Соберите сокровища (T), избегая ловушек (X)\n";
    cout << "Управление: W - вверх, A - влево, S - вниз, D - вправо, Q - выход\n";
    cout << "Нажмите Enter чтобы начать...";
    getInput();

    while (!game.isGameOver()) {
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

        GameObject* collided_object = field.getObjectAt(player.getX(), player.getY());
        if (collided_object) {
            if (collided_object->getSymbol() == 'T') {
                player.notify(EventType::TreasureCollected);
                field.removeObject(collided_object);
                field.addTrap();
                cout << "Найдено сокровище! Появилась новая ловушка.\n";
            } else if (collided_object->getSymbol() == 'X') {
                player.notify(EventType::TrapTriggered);
                cout << "Вы наступили на ловушку!\n";
            }
        }
    }

    cout << "Спасибо за игру!\n";
    return 0;
}