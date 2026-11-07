import test from 'node:test';
import assert from 'node:assert/strict';
import {BUTTON,DEFAULT_KEYS,gamepadState,deadzone} from '../web/input.js';
test('standard controller buttons map to the Vita ABI',()=>{
  const buttons=Array.from({length:16},()=>({pressed:false}));
  buttons[0].pressed=true;buttons[4].pressed=true;buttons[14].pressed=true;
  assert.equal(gamepadState({connected:true,mapping:'standard',buttons,axes:[]}).buttons,BUTTON.CROSS|BUTTON.L|BUTTON.LEFT);
});
test('deadzone removes drift and preserves full stick reach',()=>{
  assert.equal(deadzone(.1),0);assert.equal(deadzone(-.15),0);
  assert.equal(deadzone(1),1);assert.equal(deadzone(-1),-1);
  assert.ok(Math.abs(deadzone(.58)-.5)<1e-6);
  assert.equal(deadzone(NaN),0);assert.equal(deadzone(Infinity),0);
});
test('disconnected and nonstandard pads cannot leave stuck input',()=>{
  for(const pad of [null,{connected:false},{connected:true,mapping:''}])
    assert.deepEqual(gamepadState(pad),{buttons:0,axes:[0,0,0,0]});
});
test('PC action controls map space to cross, shift to dash, and mouse buttons to attacks',()=>{
  assert.equal(BUTTON[DEFAULT_KEYS.Space], BUTTON.CROSS);
  assert.equal(BUTTON[DEFAULT_KEYS.ShiftLeft], BUTTON.R);
  assert.equal(BUTTON[DEFAULT_KEYS.ControlLeft], BUTTON.L);
  assert.equal(BUTTON[DEFAULT_KEYS.KeyF], BUTTON.CIRCLE);
  assert.equal(BUTTON[DEFAULT_KEYS.Tab], BUTTON.SELECT);
  assert.equal(BUTTON[DEFAULT_KEYS.Escape], BUTTON.START);
});
