## Table of contents
* [General info](#general-info)
* [Setup](#Setup)


## General info
Für ein Modul zum Gleitschleifen wurde mittels einer Controllino MAXI automatisiert. Mittels der seriellen USB-Schnittstelle wurde die Kommunikation zu einem Raspberry Pi geschaffen, welcher die unterschiedlichen Prozesse des Gleitschleifprozesses steuert.
<p align="center">
  <img src="https://github.com/tlucky/controllino_nachbearbeitung/blob/master/images/nachbearbeitung.jpg" width="350" title="Foto">
</p>

## Setup
Das Controllino Maxi wird mit einer Netzspannung von 24 V DC versorgt. Die Eingänge der SPS sind ausschließlich analoge Werte bedingt durch die Sensorik und können mit einer maximalen Spannung von 24 V angesteuert werden. Abstandssensor, Durchﬂusssensor, Taster und Kippsensor werden an den Anschlüssen, bezeichnet mit „A<Nr.>“, der SPS angeschlossen. Der Temperatursensor wird aufgrund seiner One-Wire Connection an ein PIN (Bezeichnung „PIN<Nr.>“) angeschlossen.
<p align="center">
  <img src="https://github.com/tlucky/controllino_nachbearbeitung/blob/master/images/technischer_Anschluss.png" width="450" title="Technischer Anschluss">
</p>
