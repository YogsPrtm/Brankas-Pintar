<?php

use Illuminate\Http\Request;
use App\Http\Controllers\Api\BrankasController;
use Illuminate\Support\Facades\Route;

Route::post('/brankas/data', [BrankasController::class, 'storeData']);
Route::post('brankas/heartbeat', [BrankasController::class, 'heartbeat']);
Route::post('/brankas/reset-alarm', [BrankasController::class, 'resetAlarm']);
Route::get('brankas/dashboard', [BrankasController::class, 'getDashboard']);
