#define DEBUG // Comment this out to disable all debug prints

const unsigned long operationDelay = 3000; // delay before after and between each queue operation
const unsigned long operationDuration = 300000;
const unsigned long longPressDuration = 5000; // how long a button needs to be pressed for cancelation

// Pin Definitions
const int buttonMaster = 2;
const int buttonA = 3;
const int buttonB = 4;
const int buttonC = 5;

const int controlMaster = 6;
const int controlA1 = 7;
const int controlB1 = 8;
const int controlC1 = 9;

const int operationMaster = 10;
const int operationAp = 11;
const int operationBp = 12;
const int operationCp = 13;

// Timing Variables
const unsigned long debounceDelay = 50; // Debounce delay in ms

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

void setup()
{
    Serial.begin(9600);
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
}

void loop()
{
    unsigned long currentMillis = millis();

    // Check buttons
    checkButton(buttonMaster, 0, currentMillis); // Button Master is index 0
    checkButton(buttonA, 1, currentMillis);      // Button A is index 1
    checkButton(buttonB, 2, currentMillis);      // Button B is index 2
    checkButton(buttonC, 3, currentMillis);      // Button C is index 3

    #ifdef DEBUG
        printQueue();
    #endif

    // Manage the queue
    manageQueue(currentMillis);
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
