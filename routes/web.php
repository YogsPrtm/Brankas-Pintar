<?php

use App\Http\Controllers\Api\BrankasController;
use Illuminate\Support\Facades\Route;
use App\Http\Controllers\DashboardController;


Route::post('/brankas/data', [BrankasController::class, 'storeData']);
Route::get('/brankas/dashboard', [BrankasController::class, 'getDashboard']);
Route::get('/dashboard', [DashboardController::class, 'index']);
