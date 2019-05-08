* * *
### Come usare lo script per generare un'entry di GRUB #
* * *

#### 1. Inserire la partizione dei sorgenti
Questo significa dire al programma in quale partizione si trova la vostra directory dei sorgenti.
Ad esempio se siete su **/dev/hda2**, dovrete inserire **hda2**

#### 2. Inserire il mountpoint
Tale partizione ha un mountpoint (se non lo conoscete, date da shell il comando mount senza argomenti oppure date uno sguardo in **/etc/fstab**). Ad esempio potrà essere **/** o anche **/home/vostroutente** se tenete la home directory in una partizione separata.

#### 3. Attenzione al path
Il path dell'immagine di MentOS sarà calcolato automaticamente, ma per fare questo lo script dev'essere nella stessa directory dell'immagine.
Dopodiché vi basta confermare ed avere la vostra entry nel file **menu.lst**

**ATTENZIONE: Usatelo solo se sapete cosa state facendo. Lo script e' stato testato e funziona, ma non rispondiamo di eventuali blocchi del vostro computer.**

