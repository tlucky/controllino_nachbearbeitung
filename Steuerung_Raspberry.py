import serial
import time
import os
import glob
import mysql.connector

#Öffne serielle Kommunikation.
port = glob.glob('/dev/ttyACM*')[0]#Finde den richtigen Port.
controllino = serial.Serial(port, 9600, timeout = 2)#Timeout wird als 2 Sekunden definiert.

#Verbinde mit der Datenbank.
cnx = mysql.connector.connect(user="****", password="****",
                                  host="****", database="****")
cursor = cnx.cursor()

#Definiere Nachbearbeitung
mode = "Grob"

# Konstanten
time_vorpumpen = 10.0 * 60.0
time_pumpestoppen = 10.0
time_kippenunten = 20.0
time_ruettlerstarten = 40.0

# Variablen
event = 10
terminated = False
# 0: kein Error, 1:kritische Temperatur überschritten
# 2:kritischer Durchfluss überschritten, 3:Listen-Timeout
errorcode = 0

# Hilfsvariablen
clock_started = False
start_time = 0.0


def send_event():
    #Schreibe das berechnete event am seriellen Port.
    controllino.write(str(event).encode())


def listen_to_sps():
    global terminated
    global errorcode
    global data_string
    #Definiere dictionary für die Fehlercodes.
    error_codes = {
        "1": "Process error: Kritische Temperatur überschritten",
        "2": "Process error: Kritischen Durchfluss überschritten",
        "3": "Communication error: Controllino timed out in waiting"
    }
    #Einkommenden String ablesen und formatieren
    data_string = str(controllino.readline().decode('UTF-8')).strip()
    if data_string:#Falls eine Nachricht erhalten wurde.
        #Terminiertstatus und Fehlercode werden sofort abgelesen.
        terminated = bool(int(data_string[2]))
        errorcode = str(int(data_string[6]))
        #Es wird nach Fehlern überprüft.
        if errorcode != "0":
             file = open("/tmp/error.txt", "w+")
             file.write(error_codes.get(errorcode))
             file.close()
        return data_string
    else:#Falls nuchts erhalten wurde.
        #Fehler an die Visulisierung melden.
        file = open("/tmp/error.txt", "w+")
        file.write("Communication error: connection timed out")
        file.close()
        exit()

def event_calculation():
    global event
    global terminated
    global clock_started
    global start_time
    calculate_start_time = time.time()
    if (errorcode == 1) or (errorcode == 2) or (errorcode == 3):
        event = 11

    # Ausgangszustand
    if event == 10:
        if terminated:
            event = 1
            terminated = False

    # Schleifkörper einfüllen
    elif event == 1:
        if terminated:
            event = 2
            terminated = False

    # Vorpumpen
    elif event == 2:
        if not clock_started:
            start_time = time.time()
            clock_started = True

        current_time = time.time() - start_time
        if terminated and current_time > time_vorpumpen:
            clock_started = False
            event = 3
            terminated = False

    # Trommel starten
    elif event == 3:
        if not clock_started:
            start_time = time.time()
            clock_started = True

        current_time = time.time() - start_time
        if terminated and current_time > time_trommelstarten:
            clock_started = False
            event = 4
            terminated = False

    # Pumpe stoppen
    elif event == 4:
        if not clock_started:
            start_time = time.time()
            clock_started = True

        current_time = time.time() - start_time
        if terminated and current_time > time_pumpestoppen:
            clock_started = False
            event = 5
            terminated = False

    # Trommel stoppen
    elif event == 5:
        if terminated:
            event = 6
            terminated = False

    # Kippen unten
    elif event == 6:
        if not clock_started:
            start_time = time.time()
            clock_started = True

        current_time = time.time() - start_time
        if terminated and current_time > time_kippenunten:
            clock_started = False
            event = 7
            terminated = False

    # Kippen oben
    elif event == 7:
        if terminated:
            event = 8
            terminated = False

    # Rüttler starten
    elif event == 8:
        if not clock_started:
            start_time = time.time()
            clock_started = True

        current_time = time.time() - start_time
        if terminated and current_time > time_ruettlerstarten:
            clock_started = False
            event = 9
            terminated = False

    # Rüttler stoppen
    elif event == 9:
        if terminated:
            event = 11
            terminated = False

    #for testing
    if event == 101:
        if not clock_started:
            start_time = time.time()
            clock_started = True

        current_time = time.time() - start_time
        if terminated and current_time > 10.0:
            clock_started = False
            event = 102
            terminated = False
    print("Calculate: " + str(time.time()-calculate_start_time))
    

def main():
    time.sleep(3)#Stabilisiert die serielle Kommunikation
    global event
    global terminated
    read_process_parameters(mode)#Prozessparameter von der Datenbank ablesen.
    set_process_parameters()#Proessparameter zum Controllino schicken
    start_time = time.time()
    while not(terminated and event == 11):
        #Steuerung
        send_event()
        data_string = listen_to_sps()#Einkommende Daten ablesen
        event_calculation()
        communicate_data(data_string)#Daten zur Datenbank und Visualisierung übertragen
        
    cursor.close()
    print("Done!")


def communicate_data(process_information):
    #Übertrage eingekommene Daten zur Datenbank bzw Visualisierung
    
    #Definiere dictionary für die Interpretation des eingekommenen Strings.
    letters_to_names = {
        "T": "Temperatur",
        "D": "Durchfluss",
        "t": "timestamp",
        "E": "Event",
        "S": "Status",
        "A": "Abstand",
        "C": "Fehlercode"
    }
    #Ersetze Buchstaben mit entsprechendem Wort.
    for letter in ['S', 'C', 'E', 'T', 'A', 'D']:
        process_information = process_information.replace(letter, 
                                    letters_to_names.get(letter, "unknown"), 1)
    # Schreibe Daten in .txt Datei
    filename = "/tmp/Prozessdaten.txt"
    if os.path.isfile(filename) == False:
        file = open(filename, "w+")
        file.write( process_information + ";")
        file.close()
    # Schreibe Daten in der Datenbank
    data_list = process_information.split(";")
    cursor.execute("INSERT INTO Prozessdaten (" + ",".join(data_list[0:-1:2]) 
        + ") " + "VALUES (" + ",".join(data_list[1:len(data_list):2])  + ")")
    cnx.commit()#Änderungen werden gespeichert.

    
def read_process_parameters(mode):
    #Lese die Prozessparameter von der Datenbank ab.
    cursor.execute("SELECT * FROM Prozessparameter WHERE Bearbeitung = '"
                   + mode + "'")#Liest entsprechende Zeile ab.
    global process_parameters#Prozessparameter global gepseichert.
    process_parameters = cursor.fetchall()[0][1:4]
    

def set_process_parameters():
    #Setze die Prozessparameter
    global time_trommelstarten
    time_trommelstarten =  process_parameters[0] * 60#Minuten in Sekunden umrechnen
    #volume = process_parameters[1] Dieser Parameter ist im Controllino vorprogrammiert
    frequency = process_parameters[2]
    #Drehfrequenz wird zum Controllino geschickt.
    controllino.write(str(frequency).encode())
    filename = "/tmp/Prozessparameter.txt"
    file = open(filename, "w+")
    file.write("Nachbearbeitung:" + mode + ";Dauer:" + str(process_parameters[0])
    + ";Volumen:" + str(process_parameters[1]) + ";Drehfrequenz:" + str(process_parameters[2]))
    file.close()
    
main()
