<?php

namespace App\Http\Controllers;

use App\Models\Perangkat;
use Illuminate\Contracts\View\View;

class DashboardController extends Controller
{
    private const ID_PERANGKAT = 1;          
    private const BATAS_OFFLINE_DETIK = 45;  
    private const JUMLAH_RIWAYAT = 10;       

    public function index(): View
    {
        $perangkat = Perangkat::find(self::ID_PERANGKAT);

        $online = $perangkat?->last_seen?->gt(now()->subSeconds(self::BATAS_OFFLINE_DETIK)) ?? false;

        $riwayat = $perangkat ? $perangkat->logs()->latest()->take(self::JUMLAH_RIWAYAT)->get() : collect();
        
        return view('dashboard.index', compact('perangkat', 'online', 'riwayat'));
    }
}
