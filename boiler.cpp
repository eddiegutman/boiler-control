// #define DEBUG // Comment this out to disable all debug prints
#define BAUDRATE 9600
#define MIN_DURATION 20
#define MAX_DURATION 60
#define SECONDS_IN_MINUTE 60

// Serial commands
const char CMD_INIT[] = "init";
const char CMD_BUTTON_MASTER[] = "master";
const char CMD_TIMER_CONTROL[] = "timer";
const char CMD_LED_CONTROL[] = "led";
const char CMD_ON[] = "on";
const char CMD_OFF[] = "off";

// Timing configuration
int timer = 30;                                      // duration in minutes
unsigned long operationDuration = timer * SECONDS_IN_MINUTE * 1000; // duration in ms
const int operationDelay = 3000;           // delay before after and between each queue operation
const int longPressDuration = 3000;        // how long a button needs to be pressed for cancellation

// Pin Definitions
const int buttonMaster = 2;
const int buttonA = 8;
const int buttonB = 6;
const int buttonC = 4;

const int controlMaster = 3;
const int controlA1 = 9;
const int controlB1 = 7;
const int controlC1 = 5;

const int operationMaster = 10;
const int operationAp = 11;
const int operationBp = 12;
const int operationCp = 13;

// Timing Variables
const int debounceDelay = 50; // Debounce delay in ms

// Queue Variables
int queue[4] = {-1, -1, -1, -1}; // Holds button numbers in the queue
bool queueProcessing = false;    // True if a process is active
int queueSize = 0;

// Button States
bool buttonPressed[4] = {false, false, false, false};  // Master. A, B, C
bool buttonState[4] = {false, false, false, false};    // if in queue or in progress
bool wasLongPressed[4] = {false, false, false, false}; // Tracks if a long press was handled
int currentButton = 0;

// Timers
unsigned long lastPress[4] = {0, 0, 0, 0}; // Last press times for Master, A, B, C
unsigned long holdStart[4] = {0, 0, 0, 0}; // Hold start times for Master, A, B, C
unsigned long delayStartTime = 0;          // Start time for 30-second delay
unsigned long processStartTime = 0;        // Start time for current process

// LEDs Mapping
int controlLeds[4] = {controlMaster, controlA1, controlB1, controlC1};
int operationLeds[4] = {operationMaster, operationAp, operationBp, operationCp};

// Indexing
int masterIndex = 0;
int buttonAIndex = 1;
int buttonBIndex = 2;
int buttonCIndex = 3;

// Serial buffer variables
char cmd[32];
int cmdIndex;

void setup()
{
    // Initialize pins
    pinMode(buttonMaster, INPUT_PULLUP);
    pinMode(buttonA, INPUT_PULLUP);
    pinMode(buttonB, INPUT_PULLUP);
    pinMode(buttonC, INPUT_PULLUP);

    for (int i = 0; i < 4; i++)
    {
        pinMode(controlLeds[i], OUTPUT);
        pinMode(operationLeds[i], OUTPUT);
        digitalWrite(controlLeds[i], LOW);
        digitalWrite(operationLeds[i], LOW);
    }

    delay(500);
    Serial.begin(BAUDRATE);
}

void loop()
{
    unsigned long currentMillis = millis();

    // Check buttons
    checkButton(buttonMaster, masterIndex, currentMillis);
    checkButton(buttonA, buttonAIndex, currentMillis);
    checkButton(buttonB, buttonBIndex, currentMillis);
    checkButton(buttonC, buttonCIndex, currentMillis);

#ifdef DEBUG
    printQueue();
#endif

    // Manage the queue
    manageQueue(currentMillis);

    // Read from the serial
    readFromSerial();
}

// Check button state and enqueue if necessary
void checkButton(int buttonPin, int buttonIndex, unsigned long currentMillis)
{
    int buttonReading = digitalRead(buttonPin); // Read the button state

    if (buttonReading == LOW) // Button is pressed
    {
        if (!buttonPressed[buttonIndex]) // First detection of the press
        {
            buttonPressed[buttonIndex] = true;      // Mark as pressed
            holdStart[buttonIndex] = currentMillis; // Start hold timer
            wasLongPressed[buttonIndex] = false;    // Reset long press flag
        }
        else if (!wasLongPressed[buttonIndex] &&
                 (currentMillis - holdStart[buttonIndex] > longPressDuration))
        {
            // Long press detected, cancel process
            cancelProcess(buttonIndex);
            wasLongPressed[buttonIndex] = true; // Mark that long press was handled
        }
    }
    else // Button is released
    {
        if (buttonPressed[buttonIndex]) // Only handle if button was previously pressed
        {
            if (!wasLongPressed[buttonIndex] &&
                (currentMillis - holdStart[buttonIndex] > debounceDelay))
            {
                // Register the button press only if it’s a valid short press
                lastPress[buttonIndex] = currentMillis;
                buttonState[buttonIndex] = true;
                enqueue(buttonIndex);                         // Add button to the queue
                digitalWrite(controlLeds[buttonIndex], HIGH); // Turn on control LED
                setSerialLedState(controlLeds[buttonIndex], CMD_ON);
            }
            // Reset states after release
            buttonPressed[buttonIndex] = false;
            holdStart[buttonIndex] = 0;
            wasLongPressed[buttonIndex] = false;
        }
    }
}

// Manage the queue and execute processes
void manageQueue(unsigned long currentMillis)
{
    if (queueProcessing)
    {

        // Check if we're in the 30-second delay phase
        if (delayStartTime > 0 && currentMillis - delayStartTime < operationDelay)
        {
            return; // Still waiting for the delay to finish
        }
        else if (delayStartTime > 0 && currentMillis - delayStartTime >= operationDelay)
        {
            // Delay is complete, start the operation
            delayStartTime = 0;
            processStartTime = currentMillis;
            digitalWrite(operationLeds[currentButton], HIGH); // Turn on operation LED
        }

        // Check if the current process is complete
        if (processStartTime > 0 && currentMillis - processStartTime >= operationDuration)
        {
            // Turn off operation and control LEDs
            digitalWrite(operationLeds[currentButton], LOW);
            digitalWrite(controlLeds[currentButton], LOW);

            // Send LED state updates over serial
            setSerialLedState(controlLeds[currentButton], CMD_OFF);

            // Reset button state
            buttonState[currentButton] = false;

            // Dequeue and proceed to the next button
            dequeue();
            queueProcessing = false;
            processStartTime = 0;
        }
    }

    // Start the next process if queue is not empty and no process is active
    if (!queueProcessing && queueSize > 0)
    {
        queueProcessing = true;
        currentButton = queue[0];

        // Start the 30-second delay before operation begins
        delayStartTime = currentMillis;
    }
}

// Cancel the process for a specific button
void cancelProcess(int buttonIndex)
{
    // If the button is currently being processed, stop it
    if (queueProcessing && queue[0] == buttonIndex)
    {
        queueProcessing = false;
        delayStartTime = 0;
        processStartTime = 0;
        digitalWrite(operationLeds[buttonIndex], LOW);
        dequeue(); // Remove from queue
    }
    else
    {
        removeFromQueue(buttonIndex);
    }
    
    digitalWrite(controlLeds[buttonIndex], LOW);
    setSerialLedState(controlLeds[buttonIndex], CMD_OFF);
    buttonState[buttonIndex] = false; // Reset button state
}

// Enqueue a button (if not already in the queue)
void enqueue(int buttonIndex)
{
    // Check if the queue is full
    if (queueSize == 4)
    {
        return;
    }

    // Check if the button is already in the queue
    for (int i = 0; i < queueSize; i++)
    {
        if (queue[i] == buttonIndex)
        {
            return;
        }
    }

    // add Master button to the queue
    if (buttonIndex == masterIndex)
    {
        // Shift queue forward to insert Master at front
        for (int i = queueSize; i > 0; i--)
        {
            queue[i] = queue[i - 1];
        }
        queue[0] = masterIndex;
        queueSize++;

        if (queueProcessing)
        {
            // Stop the current operation immediately
            digitalWrite(operationLeds[currentButton], LOW);
            buttonState[currentButton] = false;

            // reset queue
            queueProcessing = false;
            delayStartTime = 0;
            processStartTime = 0;
        }
    }
    else
    {
        // Add the normal button to the queue
        queue[queueSize] = buttonIndex;
        queueSize++;
    }
}

// Dequeue the front button
void dequeue()
{
    if (queueSize == 0)
    {
        return;
    }

    // Remove the front element and shift others left
    for (int i = 0; i < queueSize - 1; i++)
    {
        queue[i] = queue[i + 1];
    }
    queueSize--;

    // Mark the last slot as empty
    queue[queueSize] = -1;
}

// Print the current state of the queue (for debugging)
void printQueue()
{
    Serial.print("Queue: ");
    for (int i = 0; i < queueSize; i++)
    {
        Serial.print(queue[i]);
        Serial.print(" ");
    }
    Serial.println();
}

// Remove a specific button from the queue (if it exists and is not the first/processing element)
void removeFromQueue(int buttonIndex)
{
    // Check if the queue is empty
    if (queueSize == 0)
    {
        return;
    }

    // Find the button in the queue
    for (int i = 1; i < queueSize; i++) // Start from index 1 to avoid removing the first element
    {
        if (queue[i] == buttonIndex)
        {
            // Shift all subsequent elements left
            for (int j = i; j < queueSize - 1; j++)
            {
                queue[j] = queue[j + 1];
            }

            // Mark the last slot as empty
            queue[queueSize - 1] = -1;
            queueSize--;
            return;
        }
    }
}

// read commands from the serial
void readFromSerial()
{
    if (Serial.available())
    {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n')
        {
            cmd[cmdIndex] = '\0';
            cmdIndex = 0;
            execute(cmd);
        }
        else
        {
            cmd[cmdIndex++] = c;
        }
    }
}

void setSerialLedState(int ledNum, const char *state)
{
    Serial.print(CMD_LED_CONTROL);
    Serial.print(" ");
    Serial.print(ledNum);
    Serial.print(" ");
    Serial.print(state);
    Serial.println("\n");
}

// check and send the LED states over serial
void initSerialLEDS()
{
    for (int i = 0; i < 4; i++)
    {
        // Send LED state updates over serial
        setSerialLedState(controlLeds[i], digitalRead(controlLeds[i]) == LOW ? CMD_OFF : CMD_ON);
    }
}

// check and send the time state over serial
void initSerialTimerControl()
{
    Serial.print(CMD_TIMER_CONTROL);
    Serial.print(" ");
    Serial.print(timer);
    Serial.println("\n");
}

void triggerMasterButtonOnFromSerial()
{
    lastPress[masterIndex] = millis();
    buttonState[masterIndex] = true;
    enqueue(masterIndex);
    digitalWrite(controlLeds[masterIndex], HIGH);
    setSerialLedState(controlLeds[masterIndex], CMD_ON);
}

void triggerMasterButtonOffFromSerial()
{
    cancelProcess(masterIndex);
}

void setOperationDuration(int value)
{
    if (value < MIN_DURATION || value > MAX_DURATION)
    {
        return;
    }

    timer = value;                         // value in in minutes
    operationDuration = value * SECONDS_IN_MINUTE * 1000; // convert to ms
}

// execute the correct command received from serial
void execute(char *cmd)
{
    char *keyword = strtok(cmd, " "); // first word before space
    char *arg = strtok(NULL, " ");    // second word (if any)

    if (strcmp(keyword, CMD_INIT) == 0)
    {
        initSerialLEDS();
        initSerialTimerControl();
        return;
    }

    if (strcmp(keyword, CMD_BUTTON_MASTER) == 0)
    {
        if (arg != NULL && strcmp(arg, CMD_ON) == 0)
        {
            triggerMasterButtonOnFromSerial();
        }
        else if (arg != NULL && strcmp(arg, CMD_OFF) == 0)
        {
            triggerMasterButtonOffFromSerial();
        }
        return;
    }

    if (strcmp(keyword, CMD_TIMER_CONTROL) == 0 && arg != NULL)
    {
        int value = atoi(arg);
        setOperationDuration(value);
        return;
    }
}
