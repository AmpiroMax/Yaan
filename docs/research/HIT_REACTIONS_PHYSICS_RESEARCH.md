# Удары, сбивание с ног, регдолл — как это устроено в чужих играх (рисёрч 04.09.2026)

## Комментарий ответственного (daggerfall-n-62, линия персонажей)

Заказ владельца 04.09: «мне не нравится, как ты забоялся на передачу импульса,
надо физику глобально продумывать, с регдолом уже лучше звучит, но надо с
точки зрения геймдизайна ещё продумать, как это в других играх устроено:
сваливания с ног в играх Свитков, Ведьмак, в ГТА когда машина сбивает и
прочее». Вопросы владельца: что будет, если в персонажа прилетит тяжёлый
блок — оттолкнёт ли в любую сторону; закон сохранения импульса при ударе
(ударивший замедляется, ударенный получает импульс); регдолл; болванчики.

Ниже — ДОСЛОВНЫЕ выдержки из источников (правило хранения 30.08: без
суммаризации), где страница отдалась целиком (UESP через curl, Fextralife,
Grand Theft Wiki, документация Jolt, заголовки Jolt в нашем дереве). Там,
где страница не отдалась (fandom — HTTP 402, Nexus-форум, Epic-блог), приведён
текст, который вернул поисковый инструмент, — это ЕГО пересказ результатов,
помечен «(пересказ поиска)», в дизайн не класть без перепроверки.

Итог для дизайна — в docs/design/HIT_REACTIONS_PHYSICS.md.

---

## 1. The Elder Scrolls V: Skyrim — стаггер и регдолл

Источник: https://en.uesp.net/wiki/Skyrim:Combat#Staggering (получено curl,
дословно):

> When the player, a creature, or an NPC is hit by certain attacks, there is a chance that the attack's target will be staggered. Staggering interrupts whatever action was being taken by the target and temporarily stuns them, preventing any further attacks until they have recovered. When a creature or NPC is staggered, it also stops them from moving; however, you will still be able to move if you are staggered. There are numerous ways to cause staggering, but power attacks are the simplest method and the one most used by NPCs. The Impact, Power Shot, and Power Bash perks will add staggering effects to some normal attacks, and the Force Without Effort ability will improve your overall stagger percentages. Certain dragon shouts are also guaranteed to stagger their targets.

> With the third word of the Unrelenting Force shout or the Cyclone shout, or with the Staff of Hasedoki, it is possible to knock a target off its feet and put them into what's commonly referred to as a "ragdoll" state. Like staggering, once the target is ragdolled, whatever action it was taking will be interrupted and the target will be stunned. However, the target is also knocked helplessly to the ground and will have to wait a few seconds after landing before standing up and being able to fight again. If you are ragdolled by Unrelenting Force, the camera will automatically switch to third person and remain there until you have recovered. In these situations, it may appear as though you have been killed, but as long as you can still open the menu and didn't fall too far, you will be able to recover and resume combat. Most NPCs and ground-based creatures can be ragdolled with the exception of Dwarven centurions, spheres, and ballistae, dragons, horses, and mammoths.

> Power attacks can be used to deal extra damage or break through an opponent's block. They are performed by holding down, instead of tapping, the attack button. A power attack has a chance of staggering its target and consumes Stamina according to the following formula: power attack stamina cost = (40 + weapon weight * 2) * attack cost multiplier * (1 - perk effect)

> Obvious advantages to using the horse-mounted fighting style are that you are harder to attack and are much more mobile. Additionally, if enemies block your attack, you experience no knockback, and can quickly follow up with another strike. One disadvantage to mounted combat is that if your horse is killed while you are riding it, you will be thrown to the ground in a ragdoll-like fashion […]

Источник: https://en.uesp.net/wiki/Skyrim:Unrelenting_Force (curl, дословно):

> Unrelenting Force is a shout that pushes objects and entities. Weak versions of the shout stagger enemies and slightly push objects, while the strongest version will throw objects a great distance and turn enemies into a ragdoll, briefly rendering them helpless.

> Creatures immune to ragdolling are staggered with a magnitude of 1.5, but using the Ro shout effect to eliminate the ragdolling.

> Meditating on the word Fus with Paarthurnax grants you the ability Force Without Effort. This ability causes enemies to be staggered 25% more, while you are recoiled 25% less, applying to both Unrelenting Force as well as power attacks and any other form of stagger.

> Unrelenting Force can be useful when fighting an enemy who can be pushed off a high location: falling damages and kills NPCs easier than it does you.

> Fall damage is not applied to the victim until a few seconds after the target has come to a complete stop, right before they start getting back up. As such, you may find that an opponent may appear to spontaneously die with no apparent explanation after surviving a fall.

> Even with all three words of this shout and the augmentations of Dragonborn Force and Force Without Effort, the shout can, at most, stagger a dragon. It cannot put a dragon into a "ragdoll" state. This same immunity also exists for Dwarven automatons (barring Dwarven Spiders, which are not immune), mammoths, and other enemies in Skyrim.

> Those immune sometimes include creatures that fall apart when dying and entering a ragdoll state or are plot-critical NPCs located around an area with a large drop nearby.

> Draugr use two different types of this shout, though the words are still the same […]. Draugr from the Scourge to the lowest-leveled Deathlord use the version which only staggers, while the highest-level Deathlords and Death Overlords use the version which can knock you and allies off their feet.

Комментарий: в Skyrim два дискретных состояния — стаггер (анимация-реакция с
«магнитудой», обрывает действие, не двигает тело физически) и регдолл
(Havok, тело падает, встаёт через несколько секунд, урон от падения считается
по остановке). Толчок предметов — по силе (сильные версии кидают предметы
дальше). Иммунитеты — по списку существ, а не по массе. Никакого закона
сохранения импульса «ударивший замедляется» в Skyrim нет: reakция — это
anim-событие с магнитудой; «recoil» (отдача атакующему при блоке) — тоже
анимация.

## 2. The Elder Scrolls IV: Oblivion — усталость и нокдаун

Источник: https://en.uesp.net/wiki/Oblivion:Damage_Fatigue (curl, дословно):

> Decreases the target's Fatigue level by M points each second for D seconds. The total damage is M x D points. Damaged Fatigue will regenerate at ten points per second back to its full value […]

> If a target's fatigue drops below zero, it will pass out and become unconscious until its fatigue regenerates to a positive value. […]

> Using damage fatigue can be a particularly useful method for disabling an opponent, rendering them unconscious and defenseless for many seconds. Most importantly, it can immobilize enemies that are immune to paralysis, such as storm atronachs, giving it more utility than a paralysis effect.

> In Oblivion Remastered, fatigue cannot be reduced below zero. Instead, a special knockdown is triggered (Similar to a 1-second Paralyze). When the target stands up from this knockdown, their fatigue will be set to 50% of its maximum value.

Комментарий: в Oblivion падение с ног — РЕСУРСНОЕ (усталость < 0 → регдолл до
восстановления); ремастер ввёл защиту от бесконечного нокдауна (встал —
усталость 50 %). Это готовый прототип «стойкости»: ресурс, порог, защита от
стан-лока.

## 3. Dark Souls — poise (стойкость)

Источник: https://darksouls.wiki.fextralife.com/Poise (curl, дословно):

> Poise is a Stat in Dark Souls. It's an in-game statistic that increases your resistance to being staggered or stun-locked as an effect of taking hits from opponents.

> Staggered: an enemy's attack interrupts your action. High poise will allow you to perform an action to completion despite being hit by enemies in their course (example: swinging a slow weapon, or casting spells).

> Stun-locked: an enemy's attacks causes you to temporarily be paralyzed, leaving you vulnerable to consecutive hits. Poise will allow you to soak up heavier hits and increase the number of consecutive hits you can take before getting stun-locked.

> Poise works, in essence, as the maximum value of an invisible "poise bar". Whenever you're hit by an attack, that attack/weapon has an invisible "poise damage" value, which decreases your poise bar by that amount. Consecutively receiving attacks will continue to decrease the bar further. When it hits 0, your character will stagger.

> After 5 second of not being hit, Poise will instantly refill to max for you or the enemy. Wearing armor will reduce the timer to 3.4 second.

Dark Souls III (пересказ поиска, страницы fandom не отдались):
«Hyper Armor is a gameplay mechanic in Dark Souls III that applies to the
effects of Poise and activates during the attack animations of the player,
allowing them to avoid getting staggered if receiving a hit during this time.
However, "passive poise," as seen in previous installments, does not apply
anymore. […] Stagger Point = Poise Health - (Poise Damage × Poise/100).»

Комментарий: poise = невидимая полоса стойкости, у каждого удара — «урон
стойкости», обнуление → стаггер; восстановление по таймеру без ударов. DS3
добавляет «гипер-броню» только в окнах атаки. Это и есть чистая модель
«сколько импульса тело выдержит, не потеряв анимацию».

## 4. The Witcher 3 — стаггер и нокдаун Аардом

(пересказ поиска: game8.co, fextralife, Nexus-форум «Aard Knockdown
mechanics»; страницы форума не отдались)

«Aard has a 44% Base Chance to Stagger and scales with Sign Intensity: 1%
Intensity gives 0.5% higher chance to Stagger. The time it takes an enemy to
recover is dependent on power and enemy type.» «Using Aard against big and
heavy enemies will have no effect because of their immunity to knock-downs. It
can be used on flying enemies to take them down. A knocked down enemy can be
finished off with 1 stab by standing over them and interacting.» «Drowners
are very slow to recover from knockdown.» «higher-level enemies have reduced
susceptibility to knockdowns as you progress».

Комментарий: у Ведьмака реакция — ВЕРОЯТНОСТНАЯ (шанс стаггера/нокдауна от
силы знака против сопротивления цели), нокдаун — анимация падения + окно
добивания, тяжёлые враги иммунны. Физического регдолла при нокдауне живого
нет (регдолл — на смерть).

## 5. GTA IV / V, RDR — Euphoria (NaturalMotion)

Источник: https://www.grandtheftwiki.com/Euphoria (curl, дословно):

> Euphoria is a game engine animation software developed by NaturalMotion, most well known in the context of Grand Theft Auto for its incorporation into the Rockstar Advanced Game Engine (RAGE), which Grand Theft Auto IV runs on, for use in advanced character procedural animation and ragdoll physics.

> Animations under the control of Euphoria does allow control to a limited extent. This is especially noticeable when being drunk. Pushing the directional key to a certain direction will allow the player character to move towards that direction. This also applies to tumbling after being ejected from a vehicle. It is possible to indefinitely tumble with enough speed, health, and by pushing the directional key towards the direction of tumble.

> Euphoria is generally associated to live characters in RAGE, and should not be confused with ragdoll, which is associated to dead characters in RAGE. Unlike Euphoria, when characters are in ragdoll mode, the entire model will limp onto the ground and will not react with self-preservation. It is possible to enable ragdoll mode while alive through the use of modifications or trainers only.

> A predecessor in the GTA series of the Euphoria's basic animation engine is a three-point system employed in RenderWare games, where the player's head, upper body (upper torso and arms), and lower body (lower torso and legs) are individually animated based on their interaction with their surroundings.

(пересказ поиска, gta.fandom/wiki/Euphoria — HTTP 402): «Euphoria is a game
physics engine and animation software developed and created by NaturalMotion
based on Dynamic Motion Synthesis, NaturalMotion's proprietary technology for
animating 3D characters on-the-fly "based on a full simulation of the 3D
character, including body, muscles and motor nervous system".» «Pedestrians
who are knocked off-balance by a player will stumble around dynamically and
grab onto objects (or other people) in the game world for support themselves.
RAGE's Euphoria is also designed to provide more life-like animations by
giving each live pedestrian autonomy to flail, flip, attempt to stay standing
when forced against the player character or an object, land upright, or
continue to move after falling to the ground.» «The famous ragdoll makes a
pedestrian you clip roll across the hood instead of dropping like a scripted
puppet.»

RDR2 (пересказ поиска, страницы модов Nexus/RDR2Mods): «NPCs behave differently
when shot depending on each weapon, ammo and hit zone. […] Different hit zones
with significant reactions include legs, neck, head, stomach, arms, and torso.»
«NPCs may remain on the ground longer and continue reacting to additional
impacts. NPCs no longer stumble to the same direction and they spin when shot
in the shoulder area.»

Комментарий: Euphoria — АКТИВНЫЙ регдолл: тело физическое всё время удара,
но с «мышцами» (моторами суставов) и поведениями (баланс, хватание,
приземление), которые пытаются удержать позу; пассивный регдолл — только у
мёртвых. Машина бьёт реальным телом реального веса — импульс честный, тело
перекатывается через капот. Это верхняя планка; для нас реализуемая часть —
активный регдолл через моторы Jolt (DriveToPoseUsingMotors) + баланс как
поведение стопы (§11 LOCOMOTION_GROUNDED).

## 6. Что умеет наш физический движок (Jolt 5.2 в дереве)

Источник: документация Jolt 5.0.0
https://jrouwe.github.io/JoltPhysicsDocs/5.0.0/class_character_virtual.html
(дословно):

> Runtime character object. This object usually represents the player. Contrary to the Character class it doesn't use a rigid body but moves doing collision checks only (hence the name virtual).

> Update: This is the main update function. It moves the character according to its current velocity (the character is similar to a kinematic body in the sense that you set the velocity and the character will follow unless collision is blocking the way).

> GetMaxStrength: Maximum force with which the character can push other bodies (N)

> GetMass / SetMass: Character mass (kg)

Заголовок Jolt/Physics/Character/CharacterVirtual.h в нашем дереве
(build_lead/_deps/joltphysics-src, дословно):

> /// This class contains settings that allow you to override the behavior of a character's collision response
> class CharacterContactSettings
> /// True when the object can push the virtual character.
> bool mCanPushCharacter = true;
> /// True when the virtual character can apply impulses (push) the body.
> /// Note that this only works against rigid bodies. Other CharacterVirtual objects can only be moved in their own update,
> /// so you must ensure that in their OnCharacterContactAdded mCanPushCharacter is true.
> bool mCanReceiveImpulses = true;

https://jrouwe.github.io/JoltPhysicsDocs/5.0.0/class_character_contact_listener.html (дословно):

> OnAdjustBodyVelocity: Callback to adjust the velocity of a body as seen by the character. Can be adjusted to e.g. implement a conveyor belt or an inertial dampener system of a sci-fi space ship.
> OnContactAdded: Called whenever the character collides with a body.
> OnContactSolve: Called whenever a contact is being used by the solver. Allows the listener to override the resulting character velocity (e.g. by preventing sliding along certain surfaces).

https://jrouwe.github.io/JoltPhysicsDocs/5.0.0/class_ragdoll.html (дословно):

> AddToPhysicsSystem: Add bodies and constraints to the system and optionally activate the bodies.
> SetPose: Set the ragdoll to a pose (calls BodyInterface::SetPositionAndRotation to instantly move the ragdoll)
> GetPose: Get the ragdoll pose (uses the world transform of the bodies to calculate the pose)
> DriveToPoseUsingKinematics: Drive the ragdoll to a specific pose by setting velocities on each of the bodies so that it will reach inPose in inDeltaTime.
> DriveToPoseUsingMotors: Drive the ragdoll to a specific pose by activating the motors on each constraint.
> SetLinearAndAngularVelocity: Control the linear and velocity of all bodies in the ragdoll.
> AddImpulse: Add impulse to all bodies of the ragdoll (center of mass of each of them)
> GetRootTransform: Get the position and orientation of the root of the ragdoll.

Наш контракт engine/platform/physics/interfaces/IPhysics.h (дословно, выдержка):

> float push_force_n = 100.0f;
> virtual void move_character(CharacterHandle character, const glm::vec3& displacement) = 0;
> [[nodiscard]] virtual bool character_grounded(CharacterHandle character) const = 0;
> virtual void set_body_velocity(PhysicsBodyHandle body, const glm::vec3& linear, …

Комментарий: у Jolt есть всё три уровня — (а) CharacterVirtual толкает тела
силой mMaxStrength и по умолчанию МОЖЕТ БЫТЬ ТОЛКНУТ (mCanPushCharacter),
но наш контракт IPhysics не пробрасывает ни импульс персонажу, ни его
массу, ни контакт-слушатель — «тяжёлый блок прилетит и не оттолкнёт» —
это наш контракт, не движок; (б) Ragdoll с готовым набором тел и
констрейнтов из скелета, импульс всем телам, (в) DriveToPoseUsingMotors —
активный регдолл (тело физическое, но тянется к позе анимации моторами) —
то, из чего делается «Euphoria для бедных».

## 7. Distance matching / stride warping (к §11 LOCOMOTION_GROUNDED)

https://dev.epicgames.com/documentation/en-us/unreal-engine/pose-warping-in-unreal-engine (дословно):

> Stride Warping is a Pose Warping node that dynamically adjusts the animated stride of a character to match the capsule movement speed.
> By defining the speed of motion, and the relevant bones, the Stride Warping node is able to modify the spacing between foot positions, to create a dynamically adjusting stride length to match motion speed.
> This reduces the need to manually tune movement speeds to match animation playback, and reduces the dependency on blend spaces to transition between different animation sequences for different speeds of movement.
> The Orientation Warping node applies directional compensation warps to animations in motion. With this node, you can isolate and warp the leg IK bones of an animation pose to align with the dynamically updating locomotion direction of the root motion.
> Slope Warping assists in warping feet locations to match the floor normals, to create smoother transitions of locomotion animations on inclines and stairs.

(пересказ поиска по форумам Epic/Kai Locomotion, Epic-блог Lyra не отдался):
«Distance matching aims to match the right animation frame and/or play rate
with the character physics movement.» «It is not uncommon to allow some foot
sliding, since too much Distance Matching can result in very high play rate,
and too much Stride Warping may stretch the pose of the character too much.»

Комментарий: индустрия делает ровно то, что записано в §11.1 — часы клипа от
пути (distance matching) плюс варп шага; предупреждение про «слишком высокий
темп» = условие лида (а): темп ограничен полосой роли, дальше — смена роли.
