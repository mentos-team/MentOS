----
README
initfscp - Autore di questa guida: Aleksej
----

initfscp e' un programma che serve a creare un'immagine contenente il file system utilizzato da MentOS.
Consente di inserirci dei files e/o dei mountpoint (massimo 32) all'interno. 
Per far ciò digitare il comando "initfscp [-m mountpoint1 -m mountpoint2 ... -m mountpoint n] file1 file2 ... nomefs" 

dove:
- -m mountpoint1 -m mountpoint2 ... -m mountpoint n (facoltativo) indicano i mountpoint da inserire nel filesystem appena creato. 
- "file1 file2 ..." sono i files che inseriremo nel filesystem,
- "nomefs" è il nome che daremo all'immagine.
Ad esempio vogliamo inserire i file topolino, pippo e paperino nell'immagine che chiameremo disney, quindi digitiamo:
  initfscp topolino pippo paperino disney
così facendo initfscp genererà l'immagine disney.

Altri comandi disponibili:
--help (-h) per visualizzare una guida più sintetica.
--version (-v) per visualizzare la versione utilizzata attualmente di initfscp.
