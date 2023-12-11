#include <Arduino.h>

//Några parametrar för raw data
#if !defined(RAW_BUFFER_LENGTH)
#  if RAMEND <= 0x4FF || RAMSIZE < 0x4FF
#define RAW_BUFFER_LENGTH  120
#  elif RAMEND <= 0xAFF || RAMSIZE < 0xAFF 
#define RAW_BUFFER_LENGTH  400 #  else
#define RAW_BUFFER_LENGTH  750
#  endif
#define MARK_EXCESS_MICROS 20

#include <IRremote.hpp>

#define IR_RECEIVE_PIN 2 
#define IR_SEND_PIN 3
#define APPLICATION_PIN 5

int SEND_BUTTON_PIN = APPLICATION_PIN;

int DELAY_BETWEEN_REPEAT = 50;

// Lagring för den den inspelade signalen
struct storedIRDataStruct {
    IRData receivedIRData;
    // tillägg för sendRaw
    uint8_t rawCode[RAW_BUFFER_LENGTH]; // Tiderna för raw signaler
    uint8_t rawCodeLength; // Längden på koden
} sStoredIRData;

bool sSendButtonWasActive;

void storeCode();
void sendCode(storedIRDataStruct *aIRDataToSend);

void setup() {
    pinMode(SEND_BUTTON_PIN, INPUT_PULLUP);
    Serial.begin(115200);
    
    IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); //Använder den inbyggda LED:n för feedback
    Serial.print(F("Active protocols: "));
    printActiveIRProtocols(&Serial); //Printar vilka protokoll för ir signaler som är aktiva
    Serial.println(F("vid pinne " STR(IR_RECEIVE_PIN)));

    IrSender.begin(); //Starta ir-sändaren
    Serial.print(F("Ready to send ir signal at pin " STR(IR_SEND_PIN) " at button press at pin "));
    Serial.println(SEND_BUTTON_PIN);
}

void loop() {

    // Om knappen trycks ned, skicka koden
    bool tSendButtonIsActive = (digitalRead(SEND_BUTTON_PIN) == LOW); // Knappnålen är aktiv låg

    //Kontrollera knappens läge
    if (tSendButtonIsActive) {
        if (!sSendButtonWasActive) {
            Serial.println(F("Stop receiving"));
            IrReceiver.stop();
        }
        
        //Knapp tryckt -> skicka lagrad data
        Serial.print(F("Button pressed, now sending"));
        if (sSendButtonWasActive == tSendButtonIsActive) {
            Serial.print(F("repeat"));
            sStoredIRData.receivedIRData.flags = IRDATA_FLAGS_IS_REPEAT;
        } else {
            sStoredIRData.receivedIRData.flags = IRDATA_FLAGS_EMPTY;
        }
        sendCode(&sStoredIRData);
        delay(DELAY_BETWEEN_REPEAT); // Vänta lite mellan omsändningar

    } else if (sSendButtonWasActive) {
        
        //Knappen släpptes precis -> aktivera mottagning
        
        // Starta om mottagaren
        Serial.println(F("Knappen släpptes -> börja ta emot"));
        IrReceiver.start();

    } else if (IrReceiver.decode()) {
      
        //Knappen är inte nedtryckt och data finns tillgänglig -> lagra mottagen data och återuppta
        
        storeCode();
        IrReceiver.resume(); // återuppta mottagaren
    }

    sSendButtonWasActive = tSendButtonIsActive;
    delay(100);
}

// storeCode() upptäcker och lagrar IR-signaler för senare använding. Kollar även om IR-signalen är giltig eller raw, dvs att den inte matchar ett aktivt protokoll
void storeCode() {

    //Kopiera decoded data
    sStoredIRData.receivedIRData = IrReceiver.decodedIRData;

    if (sStoredIRData.receivedIRData.protocol == UNKNOWN) {
        Serial.print(F("Received unknown code, saving "));
        Serial.print(IrReceiver.decodedIRData.rawDataPtr->rawlen - 1);
        Serial.println(F(" as raw "));
        IrReceiver.printIRResultRawFormatted(&Serial, true); //resultaten i RAW-format
        sStoredIRData.rawCodeLength = IrReceiver.decodedIRData.rawDataPtr->rawlen - 1;

        //Lagra raw data i en dedikerad array för senare använding
        IrReceiver.compensateAndStoreIRResultInArray(sStoredIRData.rawCode);
    } else {
        IrReceiver.printIRResultShort(&Serial);
        IrReceiver.printIRSendUsage(&Serial);
        sStoredIRData.receivedIRData.flags = 0; // rensa flaggor - särskilt upprepning - för senare sändning
        Serial.println();
    }
}

//sendCode() sänder ut IR-data när knappen trycks ner
void sendCode(storedIRDataStruct *aIRDataToSend) {
    if (aIRDataToSend->receivedIRData.protocol == UNKNOWN /* dvs. raw */) {
        IrSender.sendRaw(aIRDataToSend->rawCode, aIRDataToSend->rawCodeLength, 38);

        Serial.print(F("raw "));
        Serial.print(aIRDataToSend->rawCodeLength);
        Serial.println(F(" marks or spaces"));
    } else {
        IrSender.write(&aIRDataToSend->receivedIRData);
        printIRResultShort(&Serial, &aIRDataToSend->receivedIRData, false);
    }
}
