import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { createContext, runInContext, SourceTextModule } from "node:vm";

const engineURL = new URL("../game/js/Cfunction.js", import.meta.url);
const zombiesURL = new URL("../game/js/CZombie.js", import.meta.url);
const plantURL = new URL("../game/js/plants/oGun.js", import.meta.url);
const engineSource = await readFile(engineURL, "utf8");
const zombiesSource = await readFile(zombiesURL, "utf8");

// Minimal DOM/audio fixtures, not a browser: game mechanics, inheritance, DOM
// helpers, plant modules, zombie damage, and queued callbacks use real source.
function element(tagName) {
	return {
		tagName,
		nodeType: 1,
		id: "",
		className: "",
		style: {},
		childNodes: [],
		parentNode: null,
		get firstChild() {
			return this.childNodes[0];
		},
		setAttribute(name, value) {
			this[name] = value;
		},
		appendChild(child) {
			child.parentNode = this;
			this.childNodes.push(child);
			return child;
		},
		removeChild(child) {
			const index = this.childNodes.indexOf(child);
			assert.notEqual(index, -1);
			this.childNodes.splice(index, 1);
			child.parentNode = null;
		},
		cloneNode() {
			const clone = element(this.tagName);
			Object.assign(clone.style, this.style);
			clone.src = this.src;
			return clone;
		},
	};
}

async function game() {
	const body = element("body");
	const find = (id, node = body) => {
		if (node.id === id) {
			return node;
		}
		for (const child of node.childNodes) {
			const match = find(id, child);
			if (match) {
				return match;
			}
		}
		return null;
	};
	const sounds = [];
	const context = createContext({
		console,
		atob,
		navigator: { platform: "X11", userAgent: "test" },
		location: { protocol: "http:" },
		document: { body, createElement: element, getElementById: find },
		PlaySound2: (name) => sounds.push(name),
		$P: [],
		$Z: [],
	});
	context.window = context;
	runInContext(engineSource, context, { filename: engineURL.pathname });
	runInContext(zombiesSource, context, { filename: zombiesURL.pathname });

	// SourceTextModule preserves ES-module strictness; stripping import/export
	// and running a sloppy script would hide the original ReferenceError.
	const modules = new Map();
	async function load(url) {
		if (modules.has(url.href)) {
			return modules.get(url.href);
		}
		const module = new SourceTextModule(await readFile(url, "utf8"), { context, identifier: url.href });
		modules.set(url.href, module);
		await module.link((specifier, parent) => load(new URL(specifier, parent.identifier)));
		return module;
	}
	const module = await load(plantURL);
	await module.evaluate();
	const { oGun } = module.namespace;
	const puff = modules.get(new URL("oPuffShroom.js", plantURL).href).namespace.oPuffShroom;

	context.oS.LF = [0, 3, 3, 3, 3, 3];
	context.oS.ZF = context.oS.LF;
	context.oS.DKind = 0;
	context.oCoord[1]();
	context.oGd.Init();
	context.oT.Init(context.oS.R);
	context.oZ.Init(context.oS.R);
	context.oP.Init();
	context.EDPZ = body;
	context.oSym.Now = 0;
	context.oSym.TQ = [];

	// Deterministically drain real oSym tasks without browser timers. Exceptions
	// deliberately propagate to the test instead of being logged and swallowed.
	function until(tick) {
		let executed = 0;
		while (context.oSym.TQ.length) {
			const next = Math.min(...context.oSym.TQ.map((task) => task.T));
			if (next > tick) {
				break;
			}
			context.oSym.Now = next;
			let index = context.oSym.TQ.length;
			while (index--) {
				const task = context.oSym.TQ[index];
				if (task.T <= next) {
					assert.ok(++executed < 5000, "task queue must not spin indefinitely");
					task.f.apply(task.f, task.ar);
					context.oSym.removeTask(index);
				}
			}
		}
		context.oSym.Now = tick;
	}
	function plant(row = 3, column = 3, Class = oGun) {
		return context.CustomSpecial(Class, row, column);
	}
	function zombie(row = 3, column = 5) {
		const target = new context.oZombie();
		target.Init(context.GetX(column), target, context.oGd.$LF, context.oS.R + 1);
		target.CustomBirth(row, column, 0, "auto");
		// The fixture does not parse HTML. Build only the shadow/body nodes that
		// the real Birth and damage callbacks require.
		const node = context.NewEle(target.id, "div", "", null, body);
		context.NewImg(0, "", "", node);
		context.NewImg(0, target.PicArr[target.NormalGif], "", node);
		target.Birth();
		return target;
	}
	const bullets = () => body.childNodes.filter((node) => node.id.startsWith("PSB"));
	return { context, plant, zombie, until, bullets, sounds, puff };
}

for (const names of [[], ["j"], ["d"], ["j", "d"]]) {
	test(`attack uses local bindings with existing globals: ${names.join(", ") || "none"}`, async () => {
		const { context, plant, until, bullets, sounds } = await game();
		const sentinels = Object.fromEntries(names.map((name) => [name, { name }]));
		Object.assign(context, sentinels);
		const shooter = plant();
		shooter.NormalAttack();
		assert.equal(bullets().length, 1);
		assert.equal(context.$(shooter.id).childNodes[1].src, "images/Plants/gun/SeaShroomShoot.gif");
		assert.equal(sounds[0], "puff");
		for (const name of ["j", "d"]) {
			assert.equal(Object.hasOwn(context, name), names.includes(name), `must not create global ${name}`);
			assert.equal(context[name], sentinels[name], `must not overwrite global ${name}`);
		}
		until(15);
		assert.equal(bullets()[0].style.visibility, "visible");
		until(108);
		assert.equal(context.$(shooter.id).childNodes[1].src, "images/Plants/gun/SeaShroom.gif");
	});
}

test("real trigger and inherited recheck keep firing for 20 damage per hit", async () => {
	const { context, plant, zombie, until } = await game();
	const shooter = plant();
	const target = zombie();
	context.oT.chkD0(target, target.R, context.oT.$[target.R], context.oT.$L[target.R]);
	assert.equal(shooter.canTrigger, 0);
	until(60);
	assert.equal(target.HP, 250);
	until(200);
	assert.equal(target.HP, 230);
	assert.ok(context.$P[shooter.id]);
});

test("a missed projectile leaves the board and is removed", async () => {
	const { plant, until, bullets } = await game();
	plant().NormalAttack();
	until(180);
	assert.equal(bullets().length, 0);
});

for (const deathTick of [0, 107]) {
	test(`plant removal at tick ${deathTick} is safe during pending animation and attack callbacks`, async () => {
		const { context, plant, zombie, until, bullets } = await game();
		const shooter = plant();
		const target = zombie();
		shooter.TriggerCheck(target, 0);
		until(deathTick);
		shooter.Die();
		assert.equal(context.$(shooter.id), null);
		assert.equal(context.$P[shooter.id], undefined);
		assert.equal(shooter.BulletEle, null);
		until(200);
		assert.equal(target.HP, 250, "the launched projectile survives its plant; no second shot is fired");
		assert.equal(bullets().length, 0);
		assert.equal(context.oSym.TQ.length, 0);
	});
}

test("target removal before impact permits cleanup and releases the attack trigger", async () => {
	const { plant, zombie, until, bullets } = await game();
	const shooter = plant();
	const target = zombie();
	shooter.TriggerCheck(target, 0);
	until(5);
	target.Die();
	until(200);
	assert.equal(bullets().length, 0);
	assert.equal(shooter.canTrigger, 1);
});

test("simultaneous plants restore their own animation independently", async () => {
	const { context, plant, until } = await game();
	const first = plant(2);
	const second = plant(3);
	first.NormalAttack();
	second.NormalAttack();
	first.Die();
	until(108);
	assert.equal(context.$(second.id).childNodes[1].src, "images/Plants/gun/SeaShroom.gif");
	assert.ok(context.$P[second.id]);
});

test("projectiles still ignore targets above ground altitude", async () => {
	const { plant, zombie, until, bullets } = await game();
	const target = zombie();
	target.Altitude = 3;
	plant().NormalAttack();
	until(180);
	assert.equal(target.HP, 270);
	assert.equal(bullets().length, 0);
});

test("adjacent Puff-shroom behavior remains unchanged", async () => {
	const { plant, zombie, until, puff } = await game();
	const target = zombie();
	plant(3, 3, puff).NormalAttack();
	until(60);
	assert.equal(target.HP, 250);
});

test("Soak-shroom stays awake and keeps the inherited short trigger range", async () => {
	const { context, plant } = await game();
	context.oS.DKind = 1;
	const shooter = plant();
	assert.equal(shooter.Sleep, 0);
	assert.equal(shooter.canTrigger, 1);
	assert.equal(shooter.oTrigger[3][0][1], Math.min(shooter.AttackedRX + 250, context.oS.W));
});
