<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use Illuminate\Http\Request;
use App\Models\Perangkat;
use App\Models\LogAktivitas;

class BrankasController extends Controller
{
    public function storeData(Request $request){

        // validasi format data json
        $validated = $request->validate([
            'perangkat_id' => 'required|integer',
            'aktivitas' => 'required|string',
            'sumber' => 'required|string',
            'hasil' => 'required|string',
            'status_pintu' => 'required|in:terkunci,terbuka',
            'status_alarm' => 'required|in:aman_bahaya'
        ]);

        // update status terkini
        $perangkat = Perangkat::find($validated['perangkat_id']);
        if($perangkat){
            $perangkat->update([
                'status_pintu' => $validated['status_pintu'],
                'status_alarm' => $validated['status_alarm']
            ]);
        } else {
            return response()->json([
                'status' => 'error',
                'message' => 'Perangkat tidak ditemukan'
            ], 404);
        }

        // catat kejadian ke log

        LogAktivitas::create([
            'perangkat_id' => $validated['perangkat_id'],
            'aktivitas' => $validated['aktivitas'],
            'sumber' => $validated['sumber'],
            'hasil' => $validated['hasil']
        ]);

        return response()->json([
            'status' => 'success',
            'message' => 'Log dan status berhasil diperbarui'
        ], 201);
    }   

    public function getDashboard(){
        // ambil status perangkat

        $perangkat = Perangkat::find(1);

        if(!$perangkat){
            return response()->json([
                'status' => 'error',
                'message' => 'Sistem belum dikonfigurasi. Data perangkat kosong.'
            ], 404);
        }

        // ambil data riwayat maks 10

        $logs = LogAktivitas::where('perangkat_id', 1)->orderBy('created_at', 'desc')->take(10)->get();
        return response()->json([
            'status' => 'success',
            'data'   => [
                'status_terkini' => $perangkat,
                'riwayat'        => $logs
            ]
        ], 200);
    }
}
