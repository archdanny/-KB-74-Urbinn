# Portfolio Applied Data Science
---

## Inhoud

1. [Urbinn](#urbinn)
2. [Producten](#producten) <br>
2.1 [Scrum](#scrum) <br>
2.1 [Issues](#issues) <br>
2.3 [Overige](#overige) <br>
3. [Presentaties & Blog updates](#notulen_en_Presentaties)
4. [Cursussen & Workshops](#cursussen_en_workshops) <br>
4.1 [Datacamp](#datacamp) <br>
4.2 [Coursera](#coursera) <br>
4.3 [Calling Bullshit](#calling_bullshit) <br>
4.4 [Notebooks](#notebooks) <br>
4.5 [Spark](#spark) <br>

# 1. Urbinn

URBINN is het LearningLab rondom autonoom rijdend vervoer binnen stedelijke gebieden (last mile). Binnen URBINN wordt een autonoom rijdend voertuig ontwikkeld dat als basis zal dienen om steeds verder te ontwikkelen en waar allerlei onderzoek op en mee gedaan zal gaan worden. URBINN is als LearningLab gelieerd aan het Lectoraat Smart Sensor Systems, terwijl de ontwikkeling van het voertuig geheel wordt gecoördineerd en gefaciliteerd binnen de Betafactory. Het onderzoek van URBINN sluit daarbij aan bij het onderzoeksplatform : “Goed Bestuur in een Veilige Wereld”.

In de portfolio geef ik  aan wat ik allemaal voor het project heb gedaan. Hier komen issues, opdrachten en courses aan bod.

---

# 2. Producten <a name="producten"></a> 


## 2.1 Scrum

De eerste week hadden we een scrum workshop waar we de principes van scrum konden leren. We moesten ook een opdracht ervoor maken. In deze opdracht heb ik beschreven hoe scrum aan de ‘Principles behind the Agile Manifesto’ voldoet. <br>
[Scrum opdracht](Producten/Scrum/Scrum.md)


## 2.2 Issues

Hier worden de issues die ik gemaakt heb besproken.

### Issue 1 :  SVO Paper bestuderen [#1](https://github.com/urbinn/urbinn/issues/1) 
Voor deze issue heb ik bestaande technologieeen opgezocht die gebruikt worden bij zelfrijdende auto's. Ik heb naar bekend voorbeelden gezocht zoals Google Self Driving Cars en Tesla. 

### Issue 21 :  SVO Paper bestuderen [#21](https://github.com/urbinn/urbinn/issues/21) 

### Issue 27 :  SVO Paper bestuderen [#27](https://github.com/urbinn/urbinn/issues/27) 


### Issue 32 :  Camera Calibratie [#32](https://github.com/urbinn/urbinn/issues/32) 

### Issue 40 :  ORB2 save/load map [#40](https://github.com/urbinn/urbinn/issues/40) 

### Issue 44 :  Installeren ORB2 SLAM server [#44](https://github.com/urbinn/urbinn/issues/44)

### Issue 55 :  Object detection papers lezen [#55](https://github.com/urbinn/urbinn/issues/55)

### Issue 58 :  Beschrijven data structuur MapPoint in ORB2 [#58](https://github.com/urbinn/urbinn/issues/58)

### Issue 60 :  Beschrijven structuur KeyFrameDatabase in ORB2 [#60](https://github.com/urbinn/urbinn/issues/60)


### Issue 71 :  Resutaten ORB2 bin exporteren [#71](https://github.com/urbinn/urbinn/issues/71)


### Issue 32 :  Camera Calibratie [#32](https://github.com/urbinn/urbinn/issues/32)

Voor het kalibreren van de camera hebben we naar verschillende mogelijkheden gekeken. Een veelgebruikte standaard is de OpenCV Camera Calibratie framework. We hebben deze in de omgeving van ROS gebruikt.


### Issue 40 :  ORB2 save/load map [#40](https://github.com/urbinn/urbinn/issues/40)

De save/ load map feature is door een andere developer gemaakt . Het is gemaakt met behulp van de BOOST C++ framework. De boost serializatie zorgt ervoor dat de Map en de KeyframeDatabase classen geserializeerd worden.  Als de applicatie opnieuw geopend wordt kunnen de classen in dezelfde staat geïmporteerd worden. Op deze manier is het mogelijk een pointcloud te updaten met verschillende runs. Deze feature maakt een map.bin file met de classes.


Issue 44 :  Installeren ORB2 SLAM server

Ik heb ORB SLAM2 samen met Jeffrey geïnstalleerd op een centrale server. (Eerste server)


Issue 51 : ORB2 extracten points naar csv file

Om de evaluatie uit te voeren hebben we de gegenereerde point cloud nodig om de nauwkeurigheid van de algoritme te bepalen. De functionaliteit was toegevoegd en gemerged met de master branch.


Issue 55 : Object detection papers lezen.

Voor het selecteren van een object detection framework moesten we twee papers doornemen.
We zijn tijdens de  group reading sessions snel achter gekomen dat YOLO (You Only Look Once) het beste was.


Documentatie ORB SLAM2:

ORB SLAM2 is een open source SLAM algoritme met een loop closure mogelijkheid. Dit is een van de belangrijke redenen waarom we ervoor hebben gekozen. De source code was volledig gedocumenteerd.  Ik heb documentatie toegevoegd aan de source code.

Issue 58 :  Beschrijven data structuur MapPoint in ORB2

Issue 60 : Beschrijven structuur KeyFrameDatabase in ORB2


Issue 72 : Resutaten ORB2 bin exporteren

ORB SLAM2 is in c++ geschreven. Om het import tool te maken moest ik een functie maken in c++ die van ORB SLAM 2 geïmporteerd kon worden in een andere programma.



Issue 81 : Literatuur scan: filteren slam met object detectie

Issue 91 : ORB2 pointcloud maken slinger








# 3 Presentaties & Blog updates

Er is een planning gemaakt waar iedereen een paar weken de blog moet updaten en de presentaties geven. 

In de presentaties folder staat elke presentatie die ik gemaakt heb. [Folder](Producten/Presentatie) <br>
Week 5 [Presentatie](Producten/Presentatie/Week 5 - Presentatie.pptx)
Week 6 [Presentatie](Producten/Presentatie/Week 6 - Presentatie.pptx)
Week 13 [Presentatie](Producten/Presentatie/Week 13 - Presentatie.pptx)
Week 14 [Presentatie](Producten/Presentatie/Week 14 - Presentatie.pptx)




# 4. Cursussen & Workshops 



