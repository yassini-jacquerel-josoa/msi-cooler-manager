# Compatibilité matérielle, capteurs et diagnostics

Ce document décrit **ce que le code fait réellement**, pas une liste de modèles certifiés.
**Aucun modèle MSI n'a été testé physiquement par les auteurs pour cette version** : les
retours de la communauté sont nécessaires avant d'affirmer quoi que ce soit (tableau à la fin).

L'interface publique `HardwareAbstraction` reste inchangée : `cpuTemperature()` et
`gpuTemperature()` renvoient `-1` en cas d'échec, `coolerBoostState()` et
`setCoolerBoost(bool)` renvoient `false`. `SysfsHardware` conserve ce contrat pour ne pas
perturber `CoolerBoostController`, mais expose en plus des accesseurs de diagnostic
(section 4) qui expliquent la cause de l'échec.

## 1. Prérequis : module noyau `msi-ec` et droits

Le Cooler Boost n'est piloté que par l'attribut sysfs suivant :

```
/sys/devices/platform/msi-ec/cooler_boost
```

Il est fourni par le module noyau **`msi-ec`** (ou `ec_sys`/ pilote EC équivalent selon la
version du noyau et la distribution) avec l'attribut **`cooler_boost`** exporté. Vérifier :

```sh
modprobe msi-ec                    # si le module est disponible
ls -l /sys/devices/platform/msi-ec/cooler_boost
cat /sys/devices/platform/msi-ec/cooler_boost          # on/off ou 1/0
echo 1 | sudo tee /sys/devices/platform/msi-ec/cooler_boost   # écriture de test
```

Si le fichier n'existe pas, que la lecture échoue ou que l'écriture est refusée, la
régulation du ventilateur ne peut pas fonctionner : le module n'est pas chargé, son
attribut n'est pas exposé pour ce modèle, ou le processus n'a pas les droits suffisants.
**Aucun contrôle EC alternatif (ioctl, DMI, ACPI brut) n'est implémenté.**

L'application elle-même exige les droits root au démarrage (voir `README.md`). Le simple
fait d'être root ne rend pas un attribut sysfs magiquement accessible si le pilote ne
l'expose pas.

## 2. Chemins sysfs interrogés

| Chemin / motif | Usage | Comportement en cas d'absence |
| --- | --- | --- |
| `/sys/class/thermal/thermal_zone*/type` | Recherche d'une zone CPU plausible | Type comparé (insensible à la casse) à `x86_pkg_temp`, `cpu_thermal`, `k10temp`, `TCPU` |
| `/sys/class/thermal/thermal_zone*/temp` | Lecture de la température CPU retenue (millidegrés Celsius) | Repli sur `thermal_zone0/temp` ; sinon `-1` et diagnostic |
| `/sys/class/hwmon/hwmon*/name`, `/device/uevent`, `/device/vendor` | Identification *heuristique* d'un capteur GPU (pilotes `amdgpu`, `radeon`, `nouveau`, `nvidia`, `i915`, `xe`, ou fournisseur PCI AMD/NVIDIA) | Capteur ignoré si le nom évoque un capteur CPU (`k10temp`, `coretemp`, `fam15h_power`) |
| `/sys/class/hwmon/hwmon*/temp*_input` | Repli GPU, en priorité `temp1_input`, puis `temp2_input`, etc. (millidegrés Celsius) | Aucune valeur → `-1` et diagnostic ; **le capteur trouvé n'est pas garanti être le GPU** |
| `/sys/devices/platform/msi-ec/cooler_boost` | Lecture et écriture de l'état du Cooler Boost (`on`/`off` ou `1`/`0`) | `false` et diagnostic explicite |

Détails importants :

- La découverte de la zone CPU et du capteur hwmon est **mise en cache**. Une recherche
  infructueuse n'est retentée qu'au bout de **60 secondes**, pour ne pas analyser tout
  `/sys` à chaque polling d'une seconde. Un chemin present mais devenu illisible est
  invalidé immédiatement.
- Les valeurs sysfs doivent être comprises entre 1 et 200 000 millidegrés (soit 0 < T ≤ 200 °C).
  En dehors de cette plage, la valeur est traitée comme invalide et **n'est pas affichée**;
  le contrôleur conserve sa dernière mesure valide.
- Aucun module noyau n'est chargé automatiquement et aucun pilote n'est installé par
  l'application.

## 3. Température GPU : `nvidia-smi` puis repli hwmon

1. **Détection unique** : au premier appel, l'exécutable `nvidia-smi` est recherché dans
   le `PATH`, puis exécuté une fois avec
   `--query-gpu=temperature.gpu --format=csv,noheader,nounits`.
   - Si la commande fonctionne, le backend `nvidia-smi` est mémorisé pour la session ; un
    GPU NVIDIA est requis, avec un pilote exposant la température (les versions et modes
    qui renvoient `N/A` sont traitées comme une sortie inexploitable).
   - Si `nvidia-smi` est absent ou ne fonctionne pas, **il n'est plus jamais relancé** :
     l'application passe au scan hwmon décrit plus haut.
   - En cas d'échec *ultérieur* de `nvidia-smi` alors qu'il avait fonctionné, un repli hwmon
     unique est tenté, puis le backend est définitivement abandonné : aucune commande
     nvidia-smi ne reste en arrière-plan et aucune nouvelle tentative n'a lieu.
2. **Timeout** : chaque exécution est limitée à **2 secondes**. En cas de dépassement, le
   processus est tué puis attendu avant la destruction du `QProcess` : aucun processus
   `nvidia-smi` résiduel ne doit subsister. Le démarrage est lui aussi limité à 2 secondes.
3. **Choix de la mesure** : la première ligne numérique exploitable de la sortie est
   utilisée, ce qui correspond au premier GPU renvoyé par `nvidia-smi` sur un système
   multi-GPU. La valeur doit être comprise entre 1 et 150 °C.

Repli hwmon — limites à connaître :

- Les cartes AMD discrètes et de nombreux APU exposent généralement `amdgpu` (ou
  `radeon`) dans hwmon ; `temp1_input` correspond habituellement à la température *edge*,
  pas forcément au capteur « GPU » que l'utilisateur attend (junction, mémoire, etc.).
- `nouveau` et les pilotes NVIDIA peuvent exposer hwmon, mais `nvidia-smi` reste la source
  préférée quand il fonctionne.
- Les GPU intégrés Intel (`i915`, `xe`) **n'exposent souvent aucune température dans hwmon**
  : l'absence de mesure GPU est alors normale, pas un bug de l'application.
- Le filtrage par nom de pilote / fournisseur PCI reste une heuristique : sur un système
  avec plusieurs capteurs, le capteur sélectionné peut être incorrect. Vérifier avec
  `sensors`, `cat /sys/class/hwmon/hwmon*/name` et les valeurs `temp*_input` avant de
  signaler un écart.
- Si aucun capteur n'est trouvé, hwmon est réexaminé au plus toutes les 60 secondes,
  sans relancer `nvidia-smi`.

## 4. Diagnostics exposés par `SysfsHardware`

Ces méthodes sont **spécifiques à `SysfsHardware`** et ne font pas partie de l'interface
virtuelle `HardwareAbstraction` (donc aucun impact sur les mocks ou sur
`CoolerBoostController`) :

| Accesseur | Contenu |
| --- | --- |
| `status()` | Concaténation des trois statuts, séparés par ` \| ` |
| `lastError()` | Erreurs courantes concaténées, ou `Aucune erreur matérielle détectée.` |
| `cpuStatus()` | Source CPU retenue ou message d'erreur CPU |
| `gpuStatus()` | Backend GPU retenu ou message d'erreur GPU |
| `coolerBoostStatus()` | Dernier résultat de lecture/écriture du Cooler Boost |

Les messages sont en français et écrits dans la sortie d'erreur **une seule fois par
changement d'état**, ce qui évite de noyer les journaux à chaque polling. Ils ne sont pas
encore affichés dans l'interface : `main.cpp`/QML devra exposer ces accesseurs plus tard.
`lastError()` est vide de tout message d'erreur quand tout fonctionne ; la valeur de
retour `-1`/`false` reste le signal d'échec pour le contrôleur.

### Signification exacte des messages

`<...>` désigne une valeur insérée (chemin, code, erreur Qt, temps restant).

| Message (préfixe ou motif) | Signification | Action suggérée |
| --- | --- | --- |
| `CPU : non interrogé.` / `GPU : non interrogé.` / `Cooler Boost : non interrogé.` | Aucun accès matériel n'a encore été tenté | Normal au démarrage |
| `CPU : température disponible (source : <chemin>).` | `temp` lu sur la zone détectée ou sur `thermal_zone0` | Aucune |
| `CPU : aucune zone thermique exploitable dans /sys/class/thermal/thermal_zone* ; vérifiez les modules x86_pkg_temp, k10temp ou cpu_thermal.` | Aucun répertoire `thermal_zone*` ne porte un type CPU connu **et** `thermal_zone0/temp` est absent/illisible | Vérifier les modules thermal du noyau et le chemin réel sur la machine |
| `CPU : aucune zone valide ; nouvelle recherche dans <N> s.` | La dernière découverte a échoué ; nouvelle tentative dans ≤ 60 s | Attendre ou corriger le pilote ; le polling reste non bloquant |
| `CPU : lecture de <chemin> impossible (<erreur Qt>).` | La zone mémorisée a disparu ou n'est plus lisible (erreur système fournie par Qt) | Vérifier `sysfs` et les droits |
| `CPU : valeur invalide lue dans <chemin> : « <valeur> ».` | Contenu non numérique, ≤ 0 ou > 200 000 m°C : mesure rejetée | Vérifier le capteur ; ne pas considérer cette valeur comme une température |
| `GPU : température disponible via nvidia-smi.` | `nvidia-smi` a répondu avec une température plausible | Aucune |
| `GPU : capteur hwmon présomptif retenu (<chemin>).` | Premier capteur hwmon heuristiquement identifié comme GPU | Vérifier qu'il correspond bien au GPU avec `sensors` |
| `GPU : température disponible via <chemin> (capteur hwmon présomptif).` | Lecture hwmon réussie, origine non garantie | Comparer avec un outil de monitoring indépendant |
| `GPU : température disponible via <chemin> (repli après échec nvidia-smi).` | `nvidia-smi` a échoué après avoir fonctionné ; lecture hwmon réussie | Vérifier l'état du pilote NVIDIA |
| `GPU : nvidia-smi indisponible (nvidia-smi introuvable dans PATH) ...` | Exécutable absent du `PATH` (attention à l'environnement de `sudo`/`pkexec`) | Installer le paquet NVIDIA/utils ou corriger le `PATH` |
| `GPU : nvidia-smi indisponible (nvidia-smi n'a pas démarré (<erreur Qt>)) ...` | `QProcess` n'a pas pu lancer l'exécutable | Vérifier les droits et erreur Qt |
| `GPU : nvidia-smi indisponible (nvidia-smi a dépassé le délai de 2 s (processus terminé)) ...` | Timeout : le processus a été tué puis attendu | Vérifier un pilote NVIDIA bloqué ou surchargé |
| `GPU : nvidia-smi indisponible (nvidia-smi a quitté avec le code <code> : <stderr>) ...` | Commande exécutée mais en échec (pilote absent, permission, erreur NVML) | Lire le message `stderr` |
| `GPU : nvidia-smi indisponible (sortie nvidia-smi inexploitable : « <sortie> ») ...` | Aucune ligne numérique valide, par exemple `N/A` | Vérifier la prise en charge de la requête par le pilote |
| `GPU : nvidia-smi indisponible (...) et aucun capteur hwmon GPU présomptif.` | Détection initiale totalement infructueuse ; température GPU indisponible | GPU sans capteur exporté (cas typique : iGPU Intel) ou pilote à installer |
| `GPU : aucun capteur disponible ; réexamen hwmon dans <N> s (nvidia-smi n'est pas relancé).` | Aucun backend utilisable ; seul hwmon sera revérifié, au plus toutes les 60 s | Brancher/activer un GPU ou corriger le pilote ; redémarrer l'appli pour retenter `nvidia-smi` |
| `GPU : nvidia-smi a échoué après détection (<raison>) ; aucun repli hwmon exploitable, nvidia-smi n'est pas relancé.` | Panne nvidia-smi en cours de route, sans solution de repli ; nvidia-smi est abandonné pour cette session | Redémarrer l'application après avoir réparé le pilote |
| `GPU : nvidia-smi a échoué après détection (<raison>) ; repli hwmon illisible (<erreur>).` | Repli hwmon trouvé mais valeur illisible | Vérifier le chemin indiqué par l'erreur |
| `GPU : <lecture impossible ou valeur invalide> ; capteur hwmon abandonné.` | Le capteur hwmon mémorisé a disparu ou délivre une valeur aberrante | Nouvelle recherche hwmon dans la limite de 60 s |
| `Cooler Boost : lecture de <chemin> impossible (<erreur Qt>) ; vérifiez le module msi-ec, l'attribut cooler_boost et les droits root.` | Attribut absent/illisible : pilote EC manquant, attribut non exposé par ce modèle ou droits insuffisants | Charger/installer `msi-ec`, vérifier le sysfs et lancer en root |
| `Cooler Boost : valeur inattendue « <valeur> » dans <chemin> (attendu : on/off/1/0).` | Attribut présent mais format non reconnu : l'état n'est pas appliqué | Vérifier la version du module EC et signaler le format |
| `Cooler Boost : écriture de <chemin> impossible (<erreur Qt>) ; vérifiez msi-ec/cooler_boost et les droits root.` | `open()` en écriture a échoué (attribut en lecture seule ou droits) | Lancer en root, vérifier le pilote |
| `Cooler Boost : écriture partielle dans <chemin> (<octets écrits>/<octets attendus>).` | `write()` n'a pas écrit la valeur entière ; commande potentiellement non appliquée | Vérifier le pilote EC |
| `Cooler Boost : vidage de l'écriture impossible sur <chemin> (<erreur Qt>).` | Écriture effectuée mais `flush()` a échoué : résultat non garanti | Vérifier le pilote EC |
| `Cooler Boost : état disponible (on/off, source : <chemin>).` / `Cooler Boost : écriture de on/off réussie (source : <chemin>).` | Lecture/écriture correcte | Aucune |

## 5. Session graphique : X11 / XWayland

L'éditeur de liens exige `xcb` et `X11`, et le raccourci global repose sur X11 :
**Wayland natif n'est pas pris en charge**. Sous une session Wayland avec XWayland,
l'interface peut démarrer, mais la capture globale de F7 peut échouer ; un repli `Alt+F7`
n'est installé que sous GNOME (via `gsettings`). Ces contraintes sont indépendantes de la
disponibilité des capteurs : un GPU correctement détecté ne rend pas le raccourci
fonctionnel sous Wayland pur.

## 6. Tableau de retours matériels — à copier/coller

Aucune ligne n'est pré-remplie avec un modèle réel : **ne pas inventer de résultat**.
Copier le tableau ci-dessous dans une issue ou un rapport :

```markdown
| Modèle MSI (ex. Katana 15 B13V) | BIOS/EC | CPU | GPU (dédié/intégré) | Noyau (`uname -r`) | Module msi-ec + attribut cooler_boost | Type zone CPU + `cat .../temp` | Source GPU (`nvidia-smi` ou chemin hwmon + valeur) | `cat/echo cooler_boost` | Session (X11/XWayland) | Résultat (OK / partiel / échec) | Remarques |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| À remplir | À remplir | À remplir | À remplir | À remplir | À remplir | À remplir | À remplir | À remplir | À remplir | À remplir | À remplir |
```

Commandes utiles pour remplir le tableau (à adapter) :

```sh
uname -r
for z in /sys/class/thermal/thermal_zone*/type; do
    printf '%s: ' "$z"; cat "$z"
    cat "${z%type}/temp" 2>/dev/null
done
nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits 2>&1
for h in /sys/class/hwmon/hwmon*; do
    printf '%s ' "$h"; cat "$h/name" 2>/dev/null
    ls "$h"/temp*_input 2>/dev/null
done
cat /sys/devices/platform/msi-ec/cooler_boost 2>&1
```

Joindre si possible la sortie de `status()` et `lastError()` (une fois exposées dans
l'interface), la distribution et la version du pilote graphique.

## 7. Limites connues et engagements

- **Aucune validation matérielle** : ce document décrit les chemins et heuristiques du
  code, il ne certifie aucun modèle MSI.
- **Cooler Boost** : uniquement via `msi-ec/cooler_boost` ; pas de repli, pas de détection
  automatique d'un autre contrôleur EC.
- **CPU** : une zone nommée autrement que les quatre types listés plus haut ne sera pas
  choisie, sauf si elle est `thermal_zone0`.
- **GPU** : `nvidia-smi` n'interroge que le premier GPU valide ; le repli hwmon est une
  heuristique et peut sélectionner un autre capteur que le GPU. Les GPU sans export
  thermique restent sans mesure.
- **Polling** : la détection est mise en cache, avec réexamen au plus toutes les 60 s ;
  un branchement à chaud peut donc mettre jusqu'à une minute avant d'être pris en compte
  (et nécessite un redémarrage pour retenter `nvidia-smi`).
- **Valeurs** : 200 °C (sysfs) et 150 °C (`nvidia-smi`) sont des bornes de plausibilité,
  pas des limites thermiques de la machine.
