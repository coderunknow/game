/**
 * Regression test for Soak-shroom (oGun) crashing the game.
 * Upstream issue: https://github.com/Plants-vs-Zombies-MODDED/game/issues/61
 *
 * Bug: `oGun.NormalAttack` assigned to undeclared identifiers `j` and `d`
 * (`((j = k.id), (d = $(j)), ...)`). `oGun.js` is an ES module, so it runs in
 * strict mode and throws `ReferenceError: j is not defined` on the first shot.
 * Because `TriggerCheck -> CheckLoop -> NormalAttack` runs synchronously inside
 * `oZ.traversalOf()` inside `oP.Monitor`'s self-rescheduling `loop()`, the throw
 * prevents `loop()` from re-queueing itself and the whole game loop dies.
 *
 * This test executes the REAL engine (`game/js/Cfunction.js`, loaded as a classic
 * sloppy script) and the REAL plant module (`game/js/plants/oGun.js`, imported as
 * an ES module, i.e. strict mode), then drives the production call path
 * `plant.TriggerCheck -> CheckLoop -> NormalAttack` with the engine's own
 * `oSym` task queue. Only the browser DOM and audio are shimmed.
 *
 * Run with: node tests/oGun-normalattack.test.mjs
 * (no dependencies; Node 18+)
 */
import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";
import { pathToFileURL } from "node:url";

const gameRoot = new URL("../game/", import.meta.url).pathname;

// ---------- minimal DOM shim (browser stand-in) ----------
const elements = new Map();
function makeElement(tag) {
	const style = {};
	Object.defineProperty(style, "cssText", {
		set(v) {
			for (const part of String(v).split(";")) {
				const i = part.indexOf(":");
				if (i > 0) {
					const key = part
						.slice(0, i)
						.trim()
						.replace(/-([a-z])/g, (_, c) => c.toUpperCase());
					style[key] = part.slice(i + 1).trim();
				}
			}
		},
		get() {
			return "";
		},
		configurable: true,
	});
	const el = {
		tagName: String(tag || "div").toUpperCase(),
		nodeType: 1,
		childNodes: [],
		style,
		attributes: {},
		parentNode: null,
		className: "",
		innerHTML: "",
		textContent: "",
		src: "",
		appendChild(c) {
			c.parentNode = el;
			el.childNodes.push(c);
			return c;
		},
		removeChild(c) {
			const i = el.childNodes.indexOf(c);
			if (i >= 0) {
				el.childNodes.splice(i, 1);
			}
			if (c.id) {
				elements.delete(c.id);
			}
			c.parentNode = null;
			return c;
		},
		setAttribute(k, v) {
			el.attributes[k] = v;
			if (k === "id") {
				el.id = String(v);
			}
		},
		getAttribute(k) {
			return el.attributes[k];
		},
		cloneNode(deep) {
			const c = makeElement(el.tagName);
			c.style = { ...el.style };
			c.attributes = { ...el.attributes };
			c.src = el.src;
			if (deep) {
				for (const ch of el.childNodes) {
					c.appendChild(ch.cloneNode(true));
				}
			}
			return c;
		},
		querySelectorAll() {
			return [];
		},
		addEventListener() {},
		replaceWith() {},
		classList: {
			add() {},
			remove() {},
		},
	};
	Object.defineProperty(el, "id", {
		get() {
			return el._id || "";
		},
		set(v) {
			if (el._id) {
				elements.delete(el._id);
			}
			el._id = String(v);
			elements.set(el._id, el);
		},
	});
	return el;
}

const documentStub = {
	body: makeElement("body"),
	getElementById(id) {
		return elements.get(String(id)) || null;
	},
	createElement(tag) {
		return makeElement(tag);
	},
	createElementNS(ns, tag) {
		return makeElement(tag);
	},
	addEventListener() {},
	querySelectorAll() {
		return [];
	},
	querySelector() {
		return null;
	},
};

globalThis.window = globalThis;
globalThis.document = documentStub;
Object.defineProperty(globalThis, "navigator", {
	value: { platform: "Test", userAgent: "NodeTest" },
	configurable: true,
});
globalThis.location = { protocol: "http:", href: "http://localhost/game/", search: "", reload() {} };
globalThis.alert = () => {};
globalThis.confirm = () => true;
globalThis.EDPZ = makeElement("div");
globalThis.EDAll = makeElement("div");
globalThis.$P = [];
globalThis.PlaySound2 = () => {};

// ---------- load the real engine (classic sloppy script) ----------
vm.runInThisContext(fs.readFileSync(gameRoot + "js/Cfunction.js", "utf8"), {
	filename: "Cfunction.js",
});

// Engine bootstrap shims: the real GetC is installed by the game's layout init
// (Cfunction.js:660/751) which does not run here; moveBullet only reads it into
// an unused local. Lawn/zombie registries mirror what level start provides.
globalThis.GetC = (x) => Math.floor((x - 15) / 80) + 1;
globalThis.$Z = [];
globalThis.oGd.$LF = [0, 1, 1, 1, 1, 1, 1];
globalThis.oGd.$Crater = [];
globalThis.oGd.$Tombstones = {};
globalThis.oS.W = 880;
globalThis.oS.R = 6;
globalThis.oT.Init(6);
globalThis.oZ.Init(6);

// oSym.Init starts timers; emulate its queue state synchronously instead.
globalThis.oSym.Now = 0;
globalThis.oSym.TQ = [];
globalThis.oSym.NowStep = 1;

function drain(ticks) {
	for (let step = 0; step < ticks; step++) {
		globalThis.oSym.Now += 1;
		const queue = globalThis.oSym.TQ;
		for (let idx = queue.length - 1; idx >= 0; idx--) {
			if (queue[idx].T <= globalThis.oSym.Now) {
				const task = queue.splice(idx, 1)[0];
				task.f.apply(task.f, task.ar);
			}
		}
	}
}

// ---------- import the real plant module (ES module = strict mode) ----------
const { oGun } = await import(pathToFileURL(gameRoot + "js/plants/oGun.js").href);

// ---------- arrange: plant + zombie, exactly as Birth/PrivateBirth do ----------
const plant = new oGun();
plant.id = "P_test";
plant.R = 1;
plant.C = 1;
plant.AttackedLX = 200;
plant.AttackedRX = 225;
plant.pixelTop = 50;
plant.pixelLeft = 180;
plant.zIndex = 10;
plant.canTrigger = 1;
plant.HP = 300;

const ele = makeElement("div");
ele.setAttribute("id", plant.id);
ele.appendChild(documentStub.createElement("img")); // shadow (childNodes[0])
const plantImg = documentStub.createElement("img"); // plant sprite (childNodes[1])
plantImg.src = plant.PicArr[plant.NormalGif];
ele.appendChild(plantImg);
globalThis.EDPZ.appendChild(ele);
plant.ele = ele;
globalThis.$P[plant.id] = plant;
globalThis.$P.length = 1;
plant.InitTrigger(plant, plant.id, 1, 1, plant.AttackedLX, plant.AttackedRX);
plant.PrivateBirth(plant);

const zombie = {
	id: "Z_1",
	R: 1,
	HP: 100,
	PZ: 1,
	Altitude: 1,
	ZX: 260,
	AttackedLX: 250,
	AttackedRX: 300,
	WalkDirection: 0,
	getPea(z, dmg) {
		z.HP -= dmg;
	},
};
globalThis.$Z[zombie.id] = zombie;
globalThis.oZ.add(zombie);

const bullets = () => globalThis.EDPZ.childNodes.filter((n) => n.id && n.id.startsWith("PSB"));

// ---------- act & assert ----------
// 1. The production trigger path must not throw (this is the regression: before
//    the fix it threw `ReferenceError: j is not defined` and killed game loop).
assert.doesNotThrow(() => {
	plant.TriggerCheck(zombie, 0);
}, "TriggerCheck -> NormalAttack must not throw");

// 2. Shot fired: attack latch closed, shoot animation applied, bullet spawned.
assert.equal(plant.canTrigger, 0, "canTrigger latched off while attacking");
assert.equal(plantImg.src, "images/Plants/gun/SeaShroomShoot.gif", "shoot gif applied");
assert.equal(bullets().length, 1, "one bullet element spawned");
assert.equal(bullets()[0].style.visibility, "hidden", "bullet starts hidden");

// 3. Bullet becomes visible and damages the zombie in range.
drain(30);
assert.equal(zombie.HP, 80, "bullet hit zombie for 20 damage");
assert.equal(bullets().length, 0, "bullet cleaned up after hit");

// 4. Zombie leaves; the shoot-animation callback must restore the idle gif and
//    the CheckLoop re-arm task must give the plant back its trigger.
globalThis.oZ.$[1].splice(0);
delete globalThis.$Z[zombie.id];
drain(130); // now at t=160: ImgSpriter callback (~t=108) and AttackCheck1 (t=140) both ran
assert.equal(plantImg.src, "images/Plants/gun/SeaShroom.gif", "idle gif restored after shot");
assert.equal(plant.canTrigger, 1, "canTrigger restored after cooldown");

console.log("ok - oGun (Soak-shroom) NormalAttack regression test passed");
