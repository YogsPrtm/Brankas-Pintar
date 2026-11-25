<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Dashboard Brankas Pintar</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@400;500;600;700;800&display=swap">
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css">
    <link rel="stylesheet" href="{{ asset('css/style.css') }}">
</head>
<body>
<main class="container py-4" style="max-width: 960px;">

    <h1 class="fw-bold mb-4">Dashboard</h1>

    @if (! $perangkat)
        <div class="alert alert-warning">
            Perangkat belum dikonfigurasi. Tambahkan data perangkat di tabel <code>perangkats</code>.
        </div>
    @else
        @php
            $terbuka    = $perangkat->status_pintu === 'terbuka';
            $alarmAktif = $perangkat->status_alarm === 'bahaya';
        @endphp

        <!-- alarm keamanan -->
        <div class="row g-4 mb-4">
            <div class="col-md-6">
                <div class="card card-box shadow-sm h-100">
                    <div class="card-body p-4">
                        <div class="text-muted small mb-2">Alarm Keamanan</div>
                        <h2 class="fw-bold mb-3 {{ $alarmAktif ? 'warna-bahaya' : '' }}">
                            {{ $alarmAktif ? 'AKTIF' : 'Nonaktif' }}
                        </h2>
                        <div class="d-flex justify-content-between small text-muted">
                            <span>Buzzer</span>
                            <span>• {{ $alarmAktif ? 'Berbunyi' : 'Kondisi Normal' }}</span>
                        </div>
                    </div>
                </div>
            </div>

            <div class="col-md-6">
                <div class="card card-box shadow-sm h-100">
                    <div class="card-body p-4">
                        <div class="text-muted small mb-2">Status Brankas</div>
                        <h2 class="fw-bold mb-3 {{ $terbuka ? 'warna-peringatan' : '' }}">
                            {{ $terbuka ? 'Terbuka' : 'Terkunci' }}
                        </h2>
                        <div class="small text-muted">
                            Pintu brankas {{ $terbuka ? 'sedang terbuka' : 'dalam kondisi terkunci' }}
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- kontrol brankas -->
        <div class="card card-box shadow-sm mb-4">
            <div class="card-body p-4">
                <h5 class="fw-bold mb-4">Kontrol Brankas</h5>

                <div class="row align-items-center g-4">
                    <div class="col-md-6 text-center">
                        <img src="{{ asset('assets/brankas.png') }}" alt="Brankas" class="logo-brankas img-fluid">
                        <div class="fw-semibold mt-2">{{ $perangkat->nama_brankas }}</div>
                    </div>

                    <div class="col-md-6 text-center kolom-koneksi">
                        <div class="text-muted small mb-1">Koneksi Brankas</div>
                        <h3 class="fw-bold mb-2 {{ $online ? 'warna-ok' : 'warna-bahaya' }}">
                            {{ $online ? 'Online' : 'Offline' }}
                        </h3>
                        <div class="small text-muted">
                            Terakhir beroperasi: {{ $perangkat->last_seen?->format('d M Y, H:i:s') ?? '-' }}
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <!-- data riwayat -->
        <div class="card card-box shadow-sm mb-4">
            <div class="card-body p-4">
                <h5 class="fw-bold mb-3">Data &amp; Riwayat</h5>
                <div class="table-responsive">
                    <table class="table align-middle mb-0">
                        <thead>
                            <tr>
                                <th>WAKTU</th>
                                <th>AKTIVITAS</th>
                                <th class="text-end">HASIL</th>
                            </tr>
                        </thead>
                        <tbody>
                            @forelse ($riwayat as $log)
                                @php
                                    $normal = preg_match('/^(berhasil|terkunci)/i', $log->hasil);
                                @endphp
                                <tr>
                                    <td>{{ $log->created_at->format('d M Y, H:i:s') }}</td>
                                    <td>{{ $log->aktivitas }}</td>
                                    <td class="text-end">
                                        <span class="badge-hasil {{ $normal ? 'badge-hasil-info' : 'badge-hasil-warning' }}">
                                            {{ $log->hasil }}
                                        </span>
                                    </td>
                                </tr>
                            @empty
                                <tr>
                                    <td colspan="3" class="text-center text-muted py-4">
                                        Belum ada data riwayat aktivitas dari perangkat IoT.
                                    </td>
                                </tr>
                            @endforelse
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
    @endif

</main>
</body>
</html>
