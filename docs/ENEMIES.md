# Catalogue des ennemis

Les valeurs sont dans `assets/data/enemies.json`. Elles correspondent au terrain
logique de 1280×720. Les coordonnées sont en pixels, les vitesses en pixels/seconde,
les durées en secondes et les angles en degrés. Les PV et vitesses demandés sont
conservés. Le reste constitue une première proposition d'équilibrage à tester
lorsque le combat sera intégré.

## État de l'intégration

Le serveur charge ce catalogue, les patterns d'attaque et de déplacement, puis
fait apparaître les ennemis depuis `levels.json`. Les PV, dégâts de contact,
projectiles, zones annoncées, mouvements, morts et scores sont actifs. Les phases
de rage et annonces des tirs des boss restent à implémenter.

## Ennemis par niveau

| Niveau | ID | Nom | PV | Vitesse de base | Rôle | Contact | Score |
|---|---:|---|---:|---:|---|---:|---:|
| Glace | 1 | chomper | 10 | 80 | Corps à corps | 1 | 100 |
| Glace | 2 | smice | 8 | 50 | Tireur | 1 | 150 |
| Glace | 3 | pigice | 13 | 30 | Poseur de zones | 1 | 250 |
| Glace | 4 | theice | 200 | 0 | Boss | 2 | 2500 |
| Bonbons | 5 | wrapped | 15 | 80 | Corps à corps | 1 | 150 |
| Bonbons | 6 | jellybean | 12 | 50 | Tireur | 1 | 225 |
| Bonbons | 7 | gummyworm | 20 | 30 | Poseur de zones | 1 | 375 |
| Bonbons | 8 | candybear | 250 | 0 | Boss | 2 | 4000 |
| Dentiste | 9 | psyring | 18 | 80 | Corps à corps | 2 | 225 |
| Dentiste | 10 | evilmask | 20 | 50 | Tireur | 2 | 325 |
| Dentiste | 11 | denturesk | 30 | 20 | Poseur de zones | 2 | 550 |
| Dentiste | 12 | carry | 500 | 100 | Boss | 2 | 10000 |

Les corps à corps poursuivent le joueur. Les tireurs et poseurs avancent vers
la gauche ; les poseurs s'arrêtent à x=200 pour rester accessibles. Le score
est attribué au joueur qui tue l'ennemi, et le record personnel est sauvegardé.

## Attaques normales

- **Smice** : une flèche visant la position du joueur toutes les 1,8 s ; vitesse
  220, dégâts 1, rayon de collision 7.
- **Jellybean** : une petite boule toutes les 1,4 s ; vitesse 260, dégâts 1,
  rayon de collision 8.
- **Evilmask** : un projectile toxique toutes les 1,1 s ; vitesse 300, dégâts 2,
  rayon de collision 9.
- **Pigice** : cercle de rayon 110, dégâts 1, pose toutes les 4 s.
- **Gummyworm** : cercle de rayon 140, dégâts 1, pose toutes les 5 s.
- **Denturesk** : cercle de rayon 175, dégâts 2, pose toutes les 6 s.

Les tirs simples visent le joueur au moment du tir et ne sont pas des missiles
à tête chercheuse. Les zones suivent les règles ci-dessous.
Les ennemis de zone (Pigice, Gummyworm, Denturesk) posent désormais leurs cercles
à la **position du joueur ciblé**, puis ces cercles restent fixes. Une annonce
**orange de 1,5 seconde**, avec un disque de progression, précède l'activation
**rouge** : sortir pendant ce délai évite les dégâts. Une zone active peut blesser
chaque joueur au plus une fois toutes les **2 secondes**.

Les poses sont espacées de **4, 5 et 6 secondes** selon l'ennemi. Limites :
**2 zones par poseur et 3 au total**. Chaque zone reste active **2 secondes**
après son avertissement, puis disparaît. Tuer le poseur supprime
immédiatement toutes ses zones, sans supprimer celles des autres poseurs. Les
poseurs avancent jusqu'à x=200 puis restent accessibles à l'écran. Un changement
de niveau nettoie aussi les zones restantes.

## Densité des vagues

Les niveaux contiennent **40, 51 et 32 ennemis** avant leur boss. Le dernier
niveau est allégé : ses vagues sont plus petites et plus espacées, les trois
petites vagues ajoutées sont retirées, et il conserve trois poseurs de zones.
Toutes les entrées restent dans la hauteur jouable.

## Boss

- **Theice** : un renfort toutes les **3 secondes**, maximum **5 ennemis ordinaires vivants**.
- **Candybear** : un renfort toutes les **5 secondes**, maximum **2 ennemis ordinaires vivants**.
- **Carry** : **aucun renfort** pendant le combat.

Le serveur compte aussi les survivants des vagues précédentes dans les limites.
Les renforts cessent dès que le boss meurt et sont nettoyés au changement de niveau.
Configuration : `boss_support` dans `assets/data/levels.json`.

### Theice — 200 PV

Il reste en **(1140, 340)**. Son sprite animé mesure environ **623 × 623 pixels**
et occupe le côté droit, avec une partie du sprite hors écran (rayon de collision 215).
Toutes les **3,6 s**, deux points de tir situés à **(-180, -170)** et **(-180, +170)**
par rapport à son centre émettent chacun un éventail de **8 boules sur 44°** vers
la gauche. Les tirs partent donc du haut et du bas, sans émission centrale.
Vitesse 210, rayon 24, dégâts 1. Sprite `spiked_ice_ball.png`.

### Candybear — 250 PV

Il reste en **(1140, 350)**, avec un sprite animé d'environ **615 × 615 pixels**
(rayon de collision 205). Le bas opaque des trois images de ses pieds se situe
vers **y=612**, dans la surface du sol de bonbons.
Toutes les **2,6 s**, il envoie **9 poings sur 100°** en direction du joueur le plus
proche, depuis **(-180, -40)** par rapport au centre. Vitesse 230, rayon 36,
dégâts 2. Sprite `candy_boss_fist.png`.

### Carry — 500 PV

Son sprite est réduit à environ **130 × 130 pixels** (rayon de collision 36).
Il reste à **x=1100**, avec des allers-retours sinusoïdaux autour de **y=360**,
entre environ **y=110 et y=610**. Sa vitesse verticale maximale est **440 pixels/s**,
avec un cycle d'environ **3,6 s**.
Toutes les **0,9 s**, il tire **4 projectiles sur 24°** vers la gauche, comme
le tir quadruple du joueur en sens inverse. Vitesse 420, rayon 9, dégâts 2.
Sprite `infected_tooth_projectile.png`. Le précédent tir létal unique est remplacé.

Ces attaques sont actives dans le moteur. Les anciennes annonces, phases de rage
et le laser de Theice ne font pas partie de cette configuration.

## Champs préparés pour le moteur

| Champ | Sens |
|---|---|
| `level` | Niveau 1, 2 ou 3 ; IDs 11–14, 21–24, 31–34 |
| `role` | `melee`, `ranged`, `area` ou `boss` |
| `contact_damage` | Dégâts de contact, à intégrer aux collisions |
| `score` | Points prévus à la mort de l'ennemi |
| `movement.type` | `chase_player`, `advance_left`, `stationary`, `vertical_patrol` |
| `movement.spawn` | Position centrale initiale du boss |
| `movement.x/min_y/max_y` | Limites de patrouille de Carry |
| `attacks` | Liste des attaques ; vide pour le corps à corps |
| `attacks[].type` | `aimed_projectile`, `aimed_fan`, `area_pulse`, `center_beam`, `radial_burst` |
| `interval` | Délai avant l'annonce suivante, après la fin de l'attaque précédente |
| `telegraph` | Durée de l'annonce, avant tout dégât |
| `count/spread_degrees` | Nombre de tirs et angle total de l'éventail ; 360 pour un cercle complet sans doublon à 360° |
| `damage/radius/duration` | Dégâts, rayon et durée d'une zone |
| `origin/direction/length/width` | Position, direction et dimensions finales du laser |
| `projectile` | Dégâts, vitesse, rayon, durée maximale et sprite du projectile |
| `projectile.one_shot` | Élimination en un impact, à implémenter |
| `flip_x` | Retourner horizontalement l'image ; les assets de tir pointent vers la droite |
| `attack_schedule` | `alternate` : jouer les attaques dans l'ordre sans chevauchement |
| `enrage.hp_ratio` | Seuil de vie déclenchant la phase renforcée, une seule fois |
| `enrage.interval_multiplier` | Multiplier les délais, sans réduire les annonces |
| `enrage.projectile_speed_multiplier` | Multiplier la vitesse des projectiles |
| `enrage.projectile_count/spread_degrees` | Remplacer les valeurs de la salve en phase renforcée |

`fire_interval` reprend le délai de l'attaque principale pour conserver le format
historique. Les nouveaux comportements devront utiliser `attacks[].interval`
comme source de vérité. Les durées d'annonce et d'activité s'ajoutent à ce délai.

Les `sprite.texture` sont relatifs à `assets/`. Les ennemis en bandes de trois
images utilisent pour l'instant la première cellule 724×724. `wrapped_candy.png`
et les projectiles utilisent l'image entière, sauf
`infected_tooth_projectile.png` qui utilise aussi une seule cellule 724×724. `sprite.scale` conserve les
proportions : la hauteur définit les tailles visuelles prévues. Le laser fait
exception : sa texture doit être ajustée à `length × width`, sans modifier la
zone de collision en fonction des marges transparentes du PNG.
