subroutine MPI_Main
  implicit none
  include 'mpif.h'

  integer :: myrank, mysize, leftnbr, rightnbr
  integer :: step
  integer :: chkpt_info, ierr
  integer :: sts(MPI_STATUS_SIZE)
  double precision, dimension(2) :: a, b

  a(1) = .1
  a(2) = .3
  b(1) = .5
  b(2) = .7

  call MPI_Init(ierr)
  call MPI_Comm_rank(MPI_COMM_WORLD, myrank, ierr)
  call MPI_Comm_size(MPI_COMM_WORLD, mysize, ierr)

  call MPI_Info_create(chkpt_info, ierr)
  call MPI_Info_set(chkpt_info, "ampi_checkpoint", "to_file=log", ierr)

  do step = 0, 6
    leftnbr = modulo((myrank+mysize-1), mysize)
    rightnbr = modulo((myrank+1), mysize)
    call MPI_Send(a, 2, MPI_DOUBLE_PRECISION, rightnbr, 0, MPI_COMM_WORLD, ierr)
    call MPI_Recv(b, 2, MPI_DOUBLE_PRECISION, leftnbr, 0, MPI_COMM_WORLD, sts, ierr)
    if (myrank==0) then
      write(*,'(A,I0,A,I0,A,F3.1,A,F3.1,A,F3.1,A,F3.1,A)') '[', myrank, ']step ', step, &
              ',a={', a(1), ',', a(2), '},b={', b(1), ',', b(2), '}'
    end if
    if (step==2) then
      call AMPI_Migrate(AMPI_INFO_LB_SYNC, ierr)
      call AMPI_Migrate(chkpt_info, ierr)
    end if
  end do

  call MPI_Info_free(chkpt_info, ierr)
  call MPI_Finalize()

end subroutine MPI_Main
