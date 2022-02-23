"use strict";

document.addEventListener("DOMContentLoaded", function(){
	// ROMボード
	const romDataField = document.getElementById("rom-data");
	const addressSelectField = document.getElementById("addr-select").format;
	// クロック・リセットボード
	const freqDisplay = document.getElementById("freq");
	const clockCountDisplay = document.getElementById("clock-count");
	const isRunningDisplay = document.getElementById("is-running");
	const upButton = document.getElementById("up");
	const downButton = document.getElementById("down");
	const runPauseButton = document.getElementById("run-pause");
	const stepButton = document.getElementById("step");
	const resetButton = document.getElementById("reset");
	// メインボード
	const pcDisplay = document.getElementById("pc");
	const instDisplay = document.getElementById("inst");
	const regDisplays = [
		document.getElementById("reg0"), document.getElementById("reg1"),
		document.getElementById("reg2"), document.getElementById("reg3")
	];
	//PORT0/1ボード
	const portDisplays = [document.getElementById("port0"), document.getElementById("port1")];
	const portInputs = [document.getElementById("portin0"), document.getElementById("portin1")];

	// ROM
	let romData = [0, 0];
	let romAddressMask = 1;

	const readRom = function(addr) {
		return romData[addr & romAddressMask];
	};

	const updateRom = function() {
		const lines = romDataField.value.replace(/\r\n/g, "\n").replace(/\r/g, "\n").split("\n");
		const addrWidth = parseInt(addressSelectField.value);
		const newData = [];
		lines.forEach(function(line) {
			const hexMatch = (/^\s*([0-9a-f]{1,2})($|[^0-9a-f])/i).exec(line);
			if (hexMatch !== null) {
				newData.push(parseInt(hexMatch[1], 16));
			} else {
				const binMatch = (/^\s*([01]{1,8})($|[^01])/i).exec(line);
				if (binMatch !== null) {
					newData.push(parseInt(binMatch[1], 2));
				}
			}
		});
		while (newData.length < (1 << addrWidth)) {
			newData.push(0);
		}
		romData = newData;
		romAddressMask = (1 << addrWidth) - 1;
	};

	// ポート
	const portSignals = [0, 0];

	for (let i = 0; i < 8; i++) {
		for (let j = 0; j < 2; j++) {
			portInputs[j].children[i].addEventListener("click", (function(idx, mask) {
				return function() {
					portSignals[idx] ^= mask;
					portInputs[idx].setAttribute("data-value", portSignals[idx].toString(16));
				};
			})(j, 1 << (7 - i)));
		}
	}

	// クロック設定用
	const clockCandidates = [1, 2, 5, 10, 100, 1000, 10000];
	let currentCandidateIndex = 0;

	// CPUの状態とクロック管理
	let cpuFreq = 1;
	let clockCount = 0;
	let pc = 0;
	const regs = [0, 0, 0, 0];
	const portOutRegs = [0, 0];
	const portModeRegs = [0, 0];
	const portInRegs = [0, 0];
	let running = false;

	const updateDisplay = function() {
		freqDisplay.textContent = "" + cpuFreq + "Hz";
		clockCountDisplay.textContent = ("000000" + clockCount).substr(-6);
		isRunningDisplay.setAttribute("data-running", running ? "1" : "0");
		pcDisplay.setAttribute("data-value", pc.toString(16));
		instDisplay.setAttribute("data-value", readRom(pc).toString(16));
		for (let i = 0; i < 4; i++) {
			regDisplays[i].setAttribute("data-value", regs[i].toString(16));
		}
		for (let i = 0; i < 2; i++) {
			portDisplays[i].setAttribute("data-value", portOutRegs[i].toString(16));
			portDisplays[i].setAttribute("data-on", portModeRegs[i].toString(16));
		}
	};

	const execute = function() {
		const inst = readRom(pc);
		const r = regs[(inst >> 6) & 3], s = regs[(inst >> 2) & 3];
		let wb = 0, npc = pc + 1;
		if (inst & 0x20) {
			if (inst & 0x10) {
				// rr11vvvv
				const v = (inst & 0x0f) | (inst & 0x08 ? 0xf0 : 0x00);
				wb = r + v;
			} else {
				// rr10vvvv
				wb = inst & 0x0f;
			}
		} else if (inst & 0x10) {
			// rr01RRff
			switch (inst & 0x03) {
				case 0:
					wb = (inst & 0x04 ? portOutRegs : portInRegs)[(inst >> 3) & 1];
					break;
				case 1:
					wb = npc;
					npc = s;
					break;
				case 2:
					(inst & 0x04 ? portModeRegs : portOutRegs)[(inst >> 3) & 1] = r & 0xff;
					wb = r;
					break;
				case 3:
					if (r != 0) npc = s;
					wb = r;
					break;
			}
		} else {
			// rr00RRff
			switch (inst & 0x03) {
				case 0: wb = s; break;
				case 1: wb = r + s; break;
				case 2: wb = ~(r & s); break;
				case 3:
					switch ((inst >> 2) & 0x03) {
						case 0: wb = r << 2; break;
						case 1: wb = r << 4; break;
						case 2: wb = r >> 1; break;
						case 3: wb = r >> 2; break;
					}
					break;
			}
		}
		regs[(inst >> 6) & 3] = wb & 0xff;
		pc = npc & 0xff;
		for (let i = 0; i < 2; i++) {
			let newValue = 0;
			for (let j = 0; j < 8; j++) {
				// 出力モードなら出力中の値、入力モードならスイッチの値が見える
				newValue |= ((portModeRegs[i] >> j) & 1 ? portOutRegs : portSignals)[i] & (1 << j);
			}
			portInRegs[i] = newValue;
		}
		clockCount++;
		if(clockCount >= 1000000) clockCount -= 1000000;
	};

	// 自動実行用
	let prevTime = null;

	const requestFrame = requestAnimationFrame ? requestAnimationFrame : function(func) {
		setTimeout(function() {
			func((new Date()).getTime());
		}, 20);
	};

	const autoStep = function(currentTime) {
		if (running) {
			if (prevTime === null) {
				prevTime = currentTime;
			} else {
				const delta = currentTime - prevTime;
				const timePerClock = 1000 / cpuFreq;
				const stepToExecute = ~~(delta / timePerClock);
				for (let i = 0; i < stepToExecute; i++) {
					execute();
				}
				updateDisplay();
				prevTime = currentTime - (delta - timePerClock * stepToExecute);
			}
			requestFrame(autoStep);
		}
	};

	// ROMのイベントハンドラの登録
	romDataField.addEventListener("change", function() {
		updateRom();
		updateDisplay();
	});
	addressSelectField.forEach(function(e) {
		e.addEventListener("change", function() {
			updateRom();
			updateDisplay();
		});
	});

	// ROMとクロック・リセットのイベントハンドラの登録
	upButton.addEventListener("click", function() {
		if (currentCandidateIndex + 1 < clockCandidates.length) {
			currentCandidateIndex++;
			cpuFreq = clockCandidates[currentCandidateIndex];
			updateDisplay();
		}
	});
	downButton.addEventListener("click", function() {
		if (currentCandidateIndex - 1 >= 0) {
			currentCandidateIndex--;
			cpuFreq = clockCandidates[currentCandidateIndex];
			updateDisplay();
		}
	});
	runPauseButton.addEventListener("click", function() {
		running = !running;
		if (running) {
			requestFrame(autoStep);
		} else {
			prevTime = null;
		}
		updateDisplay();
	});
	stepButton.addEventListener("click", function() {
		execute();
		updateDisplay();
	});
	resetButton.addEventListener("click", function() {
		clockCount = 0;
		pc = 0;
		for (let i = 0; i < 4; i++) regs[i] = 0;
		for (let i = 0; i < 2; i++) {
			portOutRegs[i] = 0;
			portModeRegs[i] = 0;
			portInRegs[i] = 0;
		}
		running = false;
		prevTime = null;
		updateDisplay();
	});

	// 初期化
	updateRom();
	updateDisplay();
});
