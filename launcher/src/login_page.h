#pragma once

namespace glide {
static const char kLocalLoginPage[] = R"GLIDE(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>Glide Aholic</title>
<style>
body{margin:0;background:#101820;color:#eee;font:18px "Segoe UI",sans-serif}
main{max-width:420px;margin:8vh auto;padding:24px}h1{margin-top:0}
label{display:block;margin:18px 0 6px}input[type=email],input[type=password]{box-sizing:border-box;width:100%;padding:10px;font:inherit}
button{padding:10px 20px;margin-top:20px;font:inherit}button:disabled{opacity:.5}
#status{min-height:3em;color:#ccd}
.community a{color:#78dcca}
[hidden]{display:none!important}small{color:#9aa}
</style></head><body><main id="login-panel"><h1>Glide Aholic</h1>
<form id="login-form" autocomplete="off">
<label for="email">Email</label><input id="email" type="email" autocomplete="off" required>
<label for="password">Password</label><input id="password" type="password" autocomplete="off" required>
<button id="submit" type="submit" disabled>Sign in</button></form>
<p id="status" role="status">Waiting for the game...</p>
<p class="community"><a href="https://discord.gg/F9wnQ5Yaw6" target="_blank" rel="noopener noreferrer">Join BugCraft on Discord</a></p>
<small>Author: Karliky / BugCraft</small></main>
<script>
(function(){
  'use strict';
  var ready=false,busy=false,bound=false,played=false,bridge=null;
  var form=document.getElementById('login-form'),submit=document.getElementById('submit');
  var status=document.getElementById('status'),email=document.getElementById('email'),password=document.getElementById('password');
  var types={undefined:0,'null':1,number:2,string:3,array:4,object:5,boolean:6};
  function cv(value){
    var type=typeof value;
    if(Array.isArray(value)){type='array';value=value.map(cv)}
    else if(type==='object'){
      if(value){var object={};Object.keys(value).forEach(function(key){if(value[key]!==undefined)object[key]=cv(value[key])});value=object}
      else type='null';
    }
    return{type:types[type],value:value};
  }
  function getBridge(){
    if(window.native&&typeof window.native.call==='function'&&typeof window.native.on==='function')return window.native;
    if(!window.engine||typeof window.engine.call!=='function'||typeof window.engine.on!=='function')return null;
    return{call:function(name,value){return window.engine.call('call',name,cv(arguments.length>1?[value]:[]))},
      on:function(name,handler){window.engine.on(name,function(args){handler.apply(null,Array.isArray(args)?args:[args])})}};
  }
  function call(name,value){return arguments.length>1?bridge.call(name,value):bridge.call(name)}
  function failed(){busy=false;submit.disabled=!ready;password.value='';status.textContent='Sign-in failed. Check your email and password, then try again.'}
  form.onsubmit=function(event){
    event.preventDefault();
    if(!ready||busy||!email.value||!password.value)return;
    busy=true;submit.disabled=true;status.textContent='Signing in...';
    var credentials={name:email.value,password:password.value};password.value='';
    try{call('rememberEmail',{remembered:false});call('rememberPass',{remembered:false});call('login',credentials)}catch(error){failed()}
  };
  function connect(){
    if(bound)return;
    bridge=getBridge();if(!bridge)return;
    bound=true;
    bridge.on('showLauncher',function(){
      var firstShow=!ready;
      ready=true;submit.disabled=busy;status.textContent='Enter your local account details.';
      if(firstShow&&!busy&&!email.value&&!password.value&&
          (!document.activeElement||document.activeElement===document.body))email.focus();
    });
    bridge.on('loginError',failed);bridge.on('connError',failed);
    bridge.on('loginSuccess',function(){if(busy&&!played){played=true;status.textContent='Entering the game...';call('play')}});
    // Local QA flow: the user authorized automatic acceptance.
    bridge.on('showEula',function(){call('acceptEula',{marketing:false})});
    call('browserReady');
  }
  if(window.engine&&window.engine.on){window.engine.on('Ready',connect);window.engine.on('ready',connect);if(window.engine._BindingsReady)connect()}
  function awaitBridge(){connect();if(!bound)setTimeout(awaitBridge,250)}
  setTimeout(awaitBridge,250);
})();
</script></body></html>)GLIDE";
}
