#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <windows.h>  // For QueryPerformanceFrequency on Windows
    #include <conio.h>  // Windows-specific functions
    #define THREAD_RETURN DWORD WINAPI
    #define THREAD_PARAM LPVOID
#elif defined(__APPLE__) || defined(__linux__)
    #include <sys/time.h>  // For gettimeofday on macOS/Linux
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/select.h>
    #include <pthread.h>  // Include the pthread library
    #define THREAD_RETURN void*
    #define THREAD_PARAM void*
#else
    #error "Unknown platform"
#endif

// Function to get high resolution frequency or timestamp
#ifdef _WIN32
long long CurrentTime() {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (count.QuadPart * 1000LL) / freq.QuadPart;
}
void CheckInput(bool *keyboardStatus) {
    if (_kbhit()) {  // Check if a key has been pressed
        int c = _getch() - 97;  // Read character immediately
        keyboardStatus[c] = true;
    }
}

#elif defined(__APPLE__) || defined(__linux__)
long long CurrentTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);  // Convert to ms
}
void set_nonblocking() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &new_termios);  // Get terminal settings
    new_termios.c_lflag &= ~ICANON;  // Disable line buffering
    new_termios.c_lflag &= ~ECHO;    // Disable echo (optional)
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);  // Apply new settings
}
bool kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(1, &fds, NULL, NULL, &tv);  // Check if input is available
}
void CheckInput(bool *keyboardStatus) {
    set_nonblocking();  // Set terminal to non-blocking mode
    if (kbhit()) {  // Check if a key has been pressed
        int c = getchar() - 97;  // Read character immediately
        keyboardStatus[c] = true;
    }
}
#endif

bool running = true;  // Control variable for stopping the input thread
bool keyboardStatus[26] = {false}; //info from a-z
const int boardSize = 16;

void Clear();
void Pause();
THREAD_RETURN PlayerInput();  // Thread function
void DrawMap(char *map);
bool KeyPressed(char key);
void MoveHead(int direction, int *playerX, int *playerY);

int main() {
    char input;
    do {
        Clear();
        printf("Press 1 to start the game\n");
        input = getchar();
    } while (input != '1');
    
    Clear();
    printf("Game Loaded...\n");
    Pause();
    Clear();

    bool playing = true;
    int msPerFrame = 400; //2.5 fps
    int direction = 0;
    long long gameStartTime = CurrentTime();
    long long startTime = CurrentTime();
    long long endTime = CurrentTime();

    //player variables
    int tailLength = 1;
    int playerX = 5, playerY = 5;
    int tailX[256] = {5}, tailY[256] = {5};
    int appleX[256] = {13}, appleY[256] = {5};
    int apples = 1;
    //map stuff
    char map[256];
    for (int i = 0; i < boardSize * boardSize; i++) {
      map[i] = '-';
    }
    map[playerX + playerY * boardSize] = 'O';
    DrawMap(map);

    //startup functions
    // Start input thread
    #ifdef _WIN32
        HANDLE inputThread = CreateThread(NULL, 0, PlayerInput, keyboardStatus, 0, NULL);
    #elif defined(__APPLE__) || defined(__linux__)
        pthread_t inputThread;
        pthread_create(&inputThread, NULL, PlayerInput, keyboardStatus);
    #endif
    while (playing)
    {
        startTime = CurrentTime();
        ///do all operations within the frame -----------------
        //player control
        if (KeyPressed('w')) {  direction = 1; }
        if (KeyPressed('s')) {  direction = 3; }
        if (KeyPressed('a')) {  direction = 2; }
        if (KeyPressed('d')) {  direction = 0; }

        //move snake
        for (int i = tailLength - 2; i > -1; i--) {
            tailX[i+1] = tailX[i];
            tailY[i+1] = tailY[i];
        }
        tailX[0] = playerX, tailY[0] = playerY;
        MoveHead(direction, &playerX, &playerY);
        //check if eat or die
        if (playerX < 0 || playerX >= boardSize || playerY < 0 || playerY >= boardSize) {
            playing = false;
        }
        for (int i = 0; i < tailLength; i++) {
            if (playerX == tailX[i] && playerY == tailY[i]) {
                playing = false;
            }
        }
        for (int i = 0; i < apples; i++) {
            if (playerX == appleX[i] && playerY == appleY[i]) {
                tailX[tailLength] = tailX[tailLength-1];
                tailY[tailLength] = tailY[tailLength-1];
                tailLength++;
                appleX[i] = rand()%boardSize;
                appleY[i] = rand()%boardSize;
            }
        }

        if (!playing) {break;}
        
        //draw map
        char map[256];
        for (int i = 0; i < boardSize * boardSize; i++) {
          map[i] = '-';
        }
        map[playerX + playerY * boardSize] = 'O';
        for (int i = 0; i < tailLength; i++) {
            map[tailX[i] + tailY[i] * boardSize] = 'o';
        }
        for (int i = 0; i < apples; i++) {
            map[appleX[i] + appleY[i] * boardSize] = '@';
        }

        //print map
        if (true)
        {
            Clear();
            DrawMap(map);
        }

        //reset keys
        for (int i = 0; i < sizeof(keyboardStatus); i++) {
            keyboardStatus[i] = false;
        }
        ///wait for rest of frame to time out ---------------
        while (endTime < startTime + msPerFrame) {
            endTime = CurrentTime();
        }
        //printf("%d ms elapsed\n", endTime - startTime);
    }
    running = false;  // Stop the input thread after game ends
    #ifdef _WIN32
        WaitForSingleObject(inputThread, INFINITE);
        CloseHandle(inputThread);
    #elif defined(__APPLE__) || defined(__linux__)
        pthread_join(inputThread, NULL);
    #endif
    printf("Game finished\nYou achieved a score of %d!\nElapsed time %llds.", tailLength-1, (endTime - gameStartTime)/1000);
    Pause();

    return 0;
}

void Clear() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void Pause() {
#ifdef _WIN32
    system("pause");
#else
    struct termios old_termios, new_termios;

    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;

    // Restore blocking mode (enable canonical mode)
    new_termios.c_lflag |= ICANON;
    new_termios.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    // Pause
    printf("Press Enter to continue...");
    getchar();

    // Restore non-blocking mode after pausing
    new_termios.c_lflag &= ~ICANON;
    new_termios.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
#endif
}
bool KeyPressed(char key) {
    return keyboardStatus[key - 97];
}
void DrawMap(char *map) {
    for (int i = 0; i < boardSize; i++)
    {
        char line[16];
        for (int j = 0; j < boardSize; j++)
        {
            line[j] = map[i*boardSize + j];
        }
        printf("%s\n", line);
    }
}
void MoveHead(int direction, int *playerX, int *playerY) {
    if (direction == 0) {
        (*playerX)++;
    } else if (direction == 2) {
        (*playerX)--;
    } else if (direction == 1) {
        (*playerY)--;
    } else if (direction == 3) {
        (*playerY)++;
    }
}
// Function to read input in a separate thread
THREAD_RETURN PlayerInput() {
    while (running) {
        CheckInput(keyboardStatus);
    }
    return 0;
}
