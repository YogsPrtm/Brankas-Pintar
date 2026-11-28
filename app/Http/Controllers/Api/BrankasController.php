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
            'id_perangkat' => 'required|integer',
            'aktivitas' => 'required|string',
            'hasil' => 'required|string',
            'status_pintu' => 'required|in:terkunci,terbuka',
            'status_alarm' => 'required|in:aman,bahaya'
        ]);

        // update status terkini
        $perangkat = Perangkat::find($validated['id_perangkat']);
        if($perangkat){
            $perangkat->update([
                'status_pintu' => $validated['status_pintu'],
                'status_alarm' => $validated['status_alarm'],
                'last_seen' => now()
            ]);
        } else {
            return response()->json([
                'status' => 'error',
                'message' => 'Perangkat tidak ditemukan'
            ], 404);
        }

        // catat kejadian ke log

        LogAktivitas::create([
            'id_perangkat' => $validated['id_perangkat'],
            'aktivitas' => $validated['aktivitas'],
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

        $isOnline = $perangkat->last_seen && $perangkat->last_seen->gt(now()->subSeconds(45));

        // ambil data riwayat maks 10

        $logs = LogAktivitas::where('id_perangkat', 1)->orderBy('created_at', 'desc')->take(10)->get();
        return response()->json([
        'status' => 'success',
        'data'   => [
            'status_terkini' => [
                'id_perangkat'   => $perangkat->id_perangkat,
                'nama_brankas'   => $perangkat->nama_brankas,
                'status_koneksi' => $isOnline ? 'aktif' : 'offline',
                'status_pintu'   => $perangkat->status_pintu,
                'status_alarm'   => $perangkat->status_alarm === 'bahaya' ? 'aktif' : 'nonaktif',
                'last_seen'      => $perangkat->last_seen?->format('d M Y, H:i:s'),
            ],
            'riwayat' => $logs->map(function ($log) {
                return [
                    'id_log'    => $log->id_log,
                    'aktivitas' => $log->aktivitas,
                    'hasil'     => $log->hasil,
                    'waktu'     => $log->created_at->format('d M Y, H:i:s'),
                ];
            }),
        ],
    ], 200);
    }

    public function heartbeat(Request $request)
    {
        $id = $request->input('id_perangkat', 1);
        $perangkat = Perangkat::find($id);

        if (!$perangkat) {
            return response()->json(['status' => 'error', 'message' => 'Perangkat tidak ditemukan'], 404);
        }

        $perangkat->update(['last_seen' => now()]);

        return response()->json([
            'status' => 'success', 
            'message' => 'Heartbeat diterima',
            'status_alarm' => $perangkat->status_alarm]
            , 200);
    }

    public function resetAlarm(Request $request)
{
    $id = $request->input('id_perangkat', 1);
    $perangkat = Perangkat::find($id);

    if (!$perangkat) {
        return response()->json([
            'status'  => 'error',
            'message' => 'Perangkat tidak ditemukan',
        ], 404);
    }

    if ($perangkat->status_alarm === 'aman') {
        return response()->json([
            'status'  => 'success',
            'message' => 'Alarm sudah dalam kondisi nonaktif',
        ], 200);
    }

    $log = $perangkat->resetAlarm('Dashboard Web');

    return response()->json([
        'status'  => 'success',
        'message' => 'Alarm berhasil direset ke status aman',
        'data'    => [
            'status_terkini' => $perangkat->fresh(),
            'log_baru'       => $log,
        ],
    ], 200);
}
}
